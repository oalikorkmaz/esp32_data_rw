#include "storage_spiffs.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/gpio.h"
#include "driver/sdspi_host.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "driver/spi_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "spi_if.h"
#include "cfg_if.h"
#include "time_if.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <errno.h>

static const char *TAG = "STORAGE_SD";

/* ---------------------------------------------------- */
/* SD Kart Donanım Pinleri */
/* ---------------------------------------------------- */
#define SD_PWR   1
#define SD_MISO  37
#define SD_MOSI  35
#define SD_SCLK  36
#define SD_CS    47
#define SD_HOST  SPI2_HOST

#define SD_MOUNT_POINT "/sdcard"

/* Global değişkenler */
static bool s_sd_mounted = false;
static sdmmc_card_t *s_card = NULL;
static spi_if_device_handle_t s_sd_spi_lock = NULL;



/* ---------------------------------------------------- */
/* Yardımcı Fonksiyon: SPI Init */
/* ---------------------------------------------------- */
esp_err_t storage_spi_init(void)
{
    spi_bus_config_t spi2_cfg = {
        .mosi_io_num = SD_MOSI,
        .miso_io_num = SD_MISO,
        .sclk_io_num = SD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_if_init(SD_HOST, &spi2_cfg));

    if (!s_sd_spi_lock) {
        ESP_ERROR_CHECK(spi_if_register_device(SD_HOST, SD_CS, &s_sd_spi_lock));
    }

    return ESP_OK;
}

/* ---------------------------------------------------- */
/* Yardımcı Fonksiyon: Klasör oluşturma */
/* ---------------------------------------------------- */
static esp_err_t create_directory_if_not_exists(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        return ESP_OK; // zaten var
    }

    if (mkdir(path, 0755) != 0) {
        ESP_LOGE(TAG, "❌ Klasör oluşturulamadı: %s (errno=%d)", path, errno);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "✅ Klasör oluşturuldu: %s", path);
    return ESP_OK;
}

/* ---------------------------------------------------- */
/* Hiyerarşik yol oluşturma: /sdcard/YYYY/MM/DD/... */
/* ---------------------------------------------------- */
static esp_err_t create_hierarchical_path(const char *timestamp, char *file_path, size_t path_len)
{
    if (strlen(timestamp) < 19) {
        ESP_LOGE(TAG, "❌ Geçersiz timestamp formatı: %s", timestamp);
        return ESP_FAIL;
    }

    char year[5], month[3], day[3];
    strncpy(year, timestamp, 4); year[4] = '\0';
    strncpy(month, timestamp + 5, 2); month[2] = '\0';
    strncpy(day, timestamp + 8, 2); day[2] = '\0';

    /* Yıl / Ay / Gün klasörleri oluşturma */
    char year_path[128], month_path[128], day_path[128];

    /* year_path = /sdcard/YYYY */
    strlcpy(year_path, SD_MOUNT_POINT, sizeof(year_path));
    strlcat(year_path, "/", sizeof(year_path));
    strlcat(year_path, year, sizeof(year_path));
    if (create_directory_if_not_exists(year_path) != ESP_OK) return ESP_FAIL;

    /* month_path = /sdcard/YYYY/MM */
    strlcpy(month_path, year_path, sizeof(month_path));
    strlcat(month_path, "/", sizeof(month_path));
    strlcat(month_path, month, sizeof(month_path));
    if (create_directory_if_not_exists(month_path) != ESP_OK) return ESP_FAIL;

    /* day_path = /sdcard/YYYY/MM/DD */
    strlcpy(day_path, month_path, sizeof(day_path));
    strlcat(day_path, "/", sizeof(day_path));
    strlcat(day_path, day, sizeof(day_path));
    if (create_directory_if_not_exists(day_path) != ESP_OK) return ESP_FAIL;

    /* Dosya adı oluşturma */
    char hour[3], min[3], sec[3];
    strncpy(hour, timestamp + 11, 2); hour[2] = '\0';
    strncpy(min, timestamp + 14, 2);  min[2]  = '\0';
    strncpy(sec, timestamp + 17, 2);  sec[2]  = '\0';

    snprintf(file_path, path_len, "%s/%s-%s-%s_%s-%s-%s.csv",
            day_path, year, month, day, hour, min, sec);


    return ESP_OK;
}

/* ---------------------------------------------------- */
/* SD Kart Başlatma */
/* ---------------------------------------------------- */
esp_err_t storage_init(void)
{
    ESP_LOGI(TAG, "📦 SD Kart başlatılıyor...");

    if (s_sd_mounted) {
        ESP_LOGW(TAG, "⚠️ SD Kart zaten mount edilmiş.");
        return ESP_OK;
    }

    // 1️⃣ SPI başlat
    ESP_ERROR_CHECK(storage_spi_init());

    if (SD_PWR >= 0) {
        gpio_config_t pwr_cfg = {
            .pin_bit_mask = 1ULL << SD_PWR,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&pwr_cfg));
        gpio_set_level(SD_PWR, 1);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    bool lock_acquired = false;
    if (s_sd_spi_lock) {
        esp_err_t lock_ret = spi_if_bus_lock_acquire(s_sd_spi_lock, pdMS_TO_TICKS(1000));
        if (lock_ret != ESP_OK) {
            ESP_LOGE(TAG, "SD kart için SPI kilidi alınamadı: %s", esp_err_to_name(lock_ret));
            return lock_ret;
        }
        lock_acquired = true;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CS;
    slot_config.host_id = SD_HOST;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_HOST;
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    esp_err_t ret = esp_vfs_fat_sdspi_mount(
        SD_MOUNT_POINT, &host, &slot_config, &mount_config, &s_card
    );

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGW(TAG, "⚠️ SD Kart takılı değil veya bağlantı hatalı!");
        } else {
            ESP_LOGE(TAG, "❌ SD Kart init hatası: %s", esp_err_to_name(ret));
        }
        if (lock_acquired) {
            spi_if_bus_lock_release(s_sd_spi_lock);
        }
        return ret;
    }

    s_sd_mounted = true;

    /* Kart Bilgilerini Göster */
    sdmmc_card_print_info(stdout, s_card);
    uint64_t card_size_mb = ((uint64_t)s_card->csd.capacity) * s_card->csd.sector_size / (1024 * 1024);

    ESP_LOGI(TAG, "✅ SD Kart mount edildi!");
    ESP_LOGI(TAG, "   Mount Point: %s", SD_MOUNT_POINT);
    ESP_LOGI(TAG, "   Kapasite   : %llu MB", card_size_mb);
    ESP_LOGI(TAG, "   Tip        : %s", (s_card->ocr & BIT(30)) ? "SDHC/SDXC" : "SDSC");

    if (lock_acquired) {
        spi_if_bus_lock_release(s_sd_spi_lock);
    }

    return ESP_OK;
}

/* ---------------------------------------------------- */
/* SD Kart Unmount */
/* ---------------------------------------------------- */
esp_err_t storage_deinit(void)
{
    if (!s_sd_mounted) {
        ESP_LOGW(TAG, "⚠️ SD Kart zaten unmount edilmiş.");
        return ESP_OK;
    }

    bool lock_acquired = false;
    if (s_sd_spi_lock) {
        esp_err_t lock_ret = spi_if_bus_lock_acquire(s_sd_spi_lock, pdMS_TO_TICKS(1000));
        if (lock_ret != ESP_OK) {
            ESP_LOGE(TAG, "SD kart kilidi alınamadı: %s", esp_err_to_name(lock_ret));
            return lock_ret;
        }
        lock_acquired = true;
    }

    esp_err_t ret = esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, s_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ SD Kart unmount başarısız: %s", esp_err_to_name(ret));
        if (lock_acquired) {
            spi_if_bus_lock_release(s_sd_spi_lock);
        }
        return ret;
    }

    s_sd_mounted = false;
    s_card = NULL;
    ESP_LOGI(TAG, "💾 SD Kart unmount edildi.");

    if (lock_acquired) {
        spi_if_bus_lock_release(s_sd_spi_lock);
    }
    return ESP_OK;
}

/* ---------------------------------------------------- */
/* Dosya Yazma */
/* ---------------------------------------------------- */
esp_err_t storage_write_file(const char *path, const void *data_buffer, size_t len, bool append)
{
    if (!s_sd_mounted) {
        ESP_LOGE(TAG, "❌ SD Kart mount edilmemiş!");
        return ESP_ERR_INVALID_STATE;
    }

    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s%s", SD_MOUNT_POINT, path);

    FILE *f = fopen(full_path, append ? "a" : "w");
    if (!f) {
        ESP_LOGE(TAG, "❌ Dosya açılamadı: %s", full_path);
        return ESP_FAIL;
    }

    size_t written = fwrite(data_buffer, 1, len, f);
    fclose(f);
    return (written == len) ? ESP_OK : ESP_FAIL;
}

/* ---------------------------------------------------- */
/* Dosya Okuma */
/* ---------------------------------------------------- */
int storage_read_file(const char *path, void *read_buffer, size_t max_len)
{
    if (!s_sd_mounted) {
        ESP_LOGE(TAG, "❌ SD Kart mount edilmemiş!");
        return -1;
    }

    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s%s", SD_MOUNT_POINT, path);

    FILE *f = fopen(full_path, "r");
    if (!f) {
        ESP_LOGE(TAG, "❌ Dosya açılamadı: %s", full_path);
        return -1;
    }

    size_t read_len = fread(read_buffer, 1, max_len, f);
    fclose(f);
    return (int)read_len;
}

/* ---------------------------------------------------- */
/* Sensör Verisi Kaydetme */
/* ---------------------------------------------------- */
esp_err_t storage_write_sensor_data(const char *timestamp, const char *label,
                                    float value, const char *unit)
{
    if (!s_sd_mounted) {
        ESP_LOGW(TAG, "⚠️ SD Kart takılı değil, veri kaydedilemedi.");
        return ESP_ERR_INVALID_STATE;
    }

    char file_path[128];
    if (create_hierarchical_path(timestamp, file_path, sizeof(file_path)) != ESP_OK)
        return ESP_FAIL;

    char line[256];
    snprintf(line, sizeof(line), "%s,%s,%.2f,%s\n", timestamp, label, value, unit);

    FILE *f = fopen(file_path, "a");
    if (!f) {
        ESP_LOGE(TAG, "❌ Dosya açılamadı: %s", file_path);
        return ESP_FAIL;
    }

    fwrite(line, 1, strlen(line), f);
    fclose(f);
    ESP_LOGI(TAG, "✅ SD Kart: %s -> %.2f %s", label, value, unit);
    return ESP_OK;
}

/* ---------------------------------------------------- */
/* Eski API Uyumluluğu */
/* ---------------------------------------------------- */
esp_err_t storage_write_data(const char *timestamp, float avg)
{
    return storage_write_sensor_data(timestamp, "Average", avg, "");
}

/* ---------------------------------------------------- */
/* SD Kart Durum Kontrolü */
/* ---------------------------------------------------- */
bool storage_is_available(void)
{
    return s_sd_mounted;
}


esp_err_t storage_write_frame(const char *frame)
{
    if (!s_sd_mounted) {
        ESP_LOGW(TAG, "⚠️ SD Kart bağlı değil, kayıt yapılmadı.");
        return ESP_ERR_INVALID_STATE;
    }

    if (!frame || strlen(frame) == 0) {
        ESP_LOGW(TAG, "⚠️ Kayıt edilecek veri boş.");
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_sd_spi_lock) return ESP_ERR_INVALID_STATE;

    esp_err_t lock_ret = spi_if_bus_lock_acquire(s_sd_spi_lock, pdMS_TO_TICKS(1000));
    if (lock_ret != ESP_OK) {
        ESP_LOGE(TAG, "[SD] LOCK alınamadı!");
        return lock_ret;
    }

    // Tarih – saat bilgisi
    char date_str[16], time_str[16];
    time_if_get_date(date_str, sizeof(date_str));   // örn: "11/11/2025"
    time_if_get_time(time_str, sizeof(time_str));   // örn: "16:52:56"

    int day = 0, month = 0, year = 0;
    int hour = 0, minute = 0, second = 0;

    // Tarih formatı "DD/MM/YYYY"
    sscanf(date_str, "%2d/%2d/%4d", &day, &month, &year);
    // Saat formatı "HH:MM:SS"
    sscanf(time_str, "%2d:%2d:%2d", &hour, &minute, &second);

    char dir_path[256];
    snprintf(dir_path, sizeof(dir_path), "%s/%04d/%02d/%02d",
            SD_MOUNT_POINT, year, month, day);

    // Klasörleri sırasıyla oluştur
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s", SD_MOUNT_POINT);
    mkdir(tmp, 0755);
    snprintf(tmp, sizeof(tmp), "%s/%04d", SD_MOUNT_POINT, year);
    mkdir(tmp, 0755);
    snprintf(tmp, sizeof(tmp), "%s/%04d/%02d", SD_MOUNT_POINT, year, month);
    mkdir(tmp, 0755);
    snprintf(tmp, sizeof(tmp), "%s/%04d/%02d/%02d", SD_MOUNT_POINT, year, month, day);
    mkdir(tmp, 0755);

    // 🔹 Dosya adı: HH-MM-SS.log
    char file_path[256];
    file_path[0] = '\0';
    strlcpy(file_path, dir_path, sizeof(file_path));
    strlcat(file_path, "/", sizeof(file_path));

    char namebuf[32];
    snprintf(namebuf, sizeof(namebuf), "%02d-%02d-%02d.log", hour, minute, second);
    strlcat(file_path, namebuf, sizeof(file_path));

    // Dosyaya yaz
    FILE *f = fopen(file_path, "a");
    if (!f) {
        ESP_LOGE(TAG, "❌ Dosya açılamadı: %s (errno=%d)", file_path, errno);
        return ESP_FAIL;
    }

    fwrite(frame, 1, strlen(frame), f);
    fclose(f);
    spi_if_bus_lock_release(s_sd_spi_lock);

    ESP_LOGI(TAG, "Frame saved to SD: %s", file_path);
    return ESP_OK;
    }
