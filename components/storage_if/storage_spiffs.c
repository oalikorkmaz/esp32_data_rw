#include "storage_if.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "driver/spi_common.h"
#include "spi_if.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <errno.h>

static const char *TAG = "STORAGE_SD";

/* ---------------------------------------------------- */
/* SD Kart Donanım Pinleri */
/* ---------------------------------------------------- */
#define SD_MISO  37
#define SD_MOSI  35
#define SD_SCLK  36
#define SD_CS    38
#define SD_SPI_HOST  SPI2_HOST

#define SD_MOUNT_POINT "/sdcard"

/* Global değişkenler */
static bool s_sd_mounted = false;
static sdmmc_card_t *s_card = NULL;



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
    return spi_if_init(SPI2_HOST, &spi2_cfg);
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

// Elle verilen yıl/ay/gün/saat ile hiyerarşiyi oluştur ve "<dir>/<HH>.log" döndür
esp_err_t storage_prepare_paths_manual(int year, int month, int day, int hour,
                                       char *out_date_dir, size_t out_date_dir_cap,
                                       char *out_hour_file, size_t out_hour_file_cap)
{
    if (!s_sd_mounted) {
        ESP_LOGW(TAG, "SD kart yok (mount edilmemiş)."); // internete devam edeceğiz
        return ESP_ERR_INVALID_STATE;
    }

    char year_path[128], month_path[128], day_path[128];

    // /sdcard/YYYY
    strlcpy(year_path, SD_MOUNT_POINT, sizeof(year_path));
    strlcat(year_path, "/", sizeof(year_path));
    char ybuf[8]; snprintf(ybuf, sizeof(ybuf), "%04d", year);
    strlcat(year_path, ybuf, sizeof(year_path));
    if (create_directory_if_not_exists(year_path) != ESP_OK) return ESP_FAIL;

    // /sdcard/YYYY/MM
    strlcpy(month_path, year_path, sizeof(month_path));
    strlcat(month_path, "/", sizeof(month_path));
    char mbuf[4]; snprintf(mbuf, sizeof(mbuf), "%02d", month);
    strlcat(month_path, mbuf, sizeof(month_path));
    if (create_directory_if_not_exists(month_path) != ESP_OK) return ESP_FAIL;

    // /sdcard/YYYY/MM/DD
    strlcpy(day_path, month_path, sizeof(day_path));
    strlcat(day_path, "/", sizeof(day_path));
    char dbuf[4]; snprintf(dbuf, sizeof(dbuf), "%02d", day);
    strlcat(day_path, dbuf, sizeof(day_path));
    if (create_directory_if_not_exists(day_path) != ESP_OK) return ESP_FAIL;

    // Çıkış 1: gün klasörü
    if (out_date_dir && out_date_dir_cap) {
        if (strnlen(day_path, sizeof(day_path)) + 1 > out_date_dir_cap) return ESP_FAIL;
        strlcpy(out_date_dir, day_path, out_date_dir_cap);
    }

    // Çıkış 2: saat dosyası (HH.log)
    if (out_hour_file && out_hour_file_cap) {
        char hbuf[4]; snprintf(hbuf, sizeof(hbuf), "%02d", hour);
        int n = snprintf(out_hour_file, out_hour_file_cap, "%s/%s.log", day_path, hbuf);
        if (n <= 0 || (size_t)n >= out_hour_file_cap) return ESP_FAIL;
    }

    ESP_LOGI(TAG, "SD yol hazır: %s  |  saat dosyası: %s",
             out_date_dir ? out_date_dir : "(yok)",
             out_hour_file ? out_hour_file : "(yok)");
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

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CS;
    slot_config.host_id = SD_SPI_HOST;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_SPI_HOST;
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

    esp_err_t ret = esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, s_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ SD Kart unmount başarısız: %s", esp_err_to_name(ret));
        return ret;
    }

    s_sd_mounted = false;
    s_card = NULL;
    ESP_LOGI(TAG, "💾 SD Kart unmount edildi.");
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
