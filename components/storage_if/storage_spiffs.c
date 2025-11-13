#include "storage_spiffs.h"

#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "driver/gpio.h"

#include "time_if.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "STORAGE_SD";

/* ------------------- PINLER ------------------- */
#define SD_PWR   1
#define SD_MISO  37
#define SD_MOSI  35
#define SD_SCLK  36
#define SD_CS    47
#define SD_HOST  SPI2_HOST

#define SD_MOUNT_POINT "/sdcard"

/* ------------------- GLOBAL ------------------- */
static bool s_sd_mounted = false;
static sdmmc_card_t *s_card = NULL;

/* ------------------- YARDIMCI ------------------- */
static esp_err_t create_directory_if_not_exists(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) return ESP_OK;

    if (mkdir(path, 0755) != 0) {
        ESP_LOGE(TAG, "mkdir failed: %s (errno=%d)", path, errno);
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void sd_power_on(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << SD_PWR,
        .mode = GPIO_MODE_OUTPUT
    };
    gpio_config(&cfg);

    gpio_set_level(SD_PWR, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
}

static void sd_power_off(void)
{
    gpio_set_level(SD_PWR, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
}

/* ------------------- SD INIT ------------------- */
esp_err_t storage_init(void)
{
    ESP_LOGI(TAG, "SD Init...");

    /* === 1) SPI2 BUS INIT === */
    spi_bus_config_t buscfg = {
        .mosi_io_num = SD_MOSI,
        .miso_io_num = SD_MISO,
        .sclk_io_num = SD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 8192
    };

    esp_err_t ret = spi_bus_initialize(SD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "SPI2 init FAIL: %s", esp_err_to_name(ret));
        return ret;
    }

    /* === 2) POWER ON (GPIO1) === */
    sd_power_on();

    /* === 3) SPI DEVICE EKLE === */
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 20 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = SD_CS,
        .queue_size = 4,
    };

    spi_device_handle_t sd_spi_dev;
    ret = spi_bus_add_device(SD_HOST, &devcfg, &sd_spi_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_add_device FAIL: %s", esp_err_to_name(ret));
        return ret;
    }

    /* === 4) SDSPI Host Config === */
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_HOST;
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.host_id = SD_HOST;
    slot_config.gpio_cs = SD_CS;

    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files = 6,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t *card;

    /* === 5) MOUNT === */
    ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_cfg, &card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD mount fail: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SD MOUNT OK");
    return ESP_OK;
}


/* ------------------- SD DEINIT ------------------- */
esp_err_t storage_deinit(void)
{
    if (!s_sd_mounted) return ESP_OK;

    esp_err_t ret = esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, s_card);
    if (ret != ESP_OK) return ret;

    s_card = NULL;
    s_sd_mounted = false;
    sd_power_off();

    return ESP_OK;
}

/* ------------------- DOSYA YAZ ------------------- */
esp_err_t storage_write_file(const char *path,
                             const void *data,
                             size_t len,
                             bool append)
{
    if (!s_sd_mounted) return ESP_ERR_INVALID_STATE;

    char full[256];
    snprintf(full, sizeof(full), "%s%s", SD_MOUNT_POINT, path);

    FILE *f = fopen(full, append ? "a" : "w");
    if (!f) return ESP_FAIL;

    fwrite(data, 1, len, f);
    fclose(f);

    return ESP_OK;
}

/* ------------------- DOSYA OKU ------------------- */
int storage_read_file(const char *path, void *buf, size_t max)
{
    if (!s_sd_mounted) return -1;

    char full[256];
    snprintf(full, sizeof(full), "%s%s", SD_MOUNT_POINT, path);

    FILE *f = fopen(full, "r");
    if (!f) return -1;

    int n = fread(buf, 1, max, f);
    fclose(f);

    return n;
}

/* ------------------- SENSOR CSV ------------------- */
static esp_err_t build_sensor_csv_path(const char *timestamp,
                                       char *out_path,
                                       size_t cap)
{
    if (strlen(timestamp) < 19)
        return ESP_ERR_INVALID_ARG;

    char y[5], m[3], d[3];
    char H[3], M[3], S[3];

    strncpy(y, timestamp, 4); y[4] = 0;
    strncpy(m, timestamp + 5, 2); m[2] = 0;
    strncpy(d, timestamp + 8, 2); d[2] = 0;

    strncpy(H, timestamp + 11, 2); H[2] = 0;
    strncpy(M, timestamp + 14, 2); M[2] = 0;
    strncpy(S, timestamp + 17, 2); S[2] = 0;

    char yp[128], mp[128], dp[128];

    /* yıl klasörü */
    strlcpy(yp, SD_MOUNT_POINT, sizeof(yp));
    strlcat(yp, "/", sizeof(yp));
    strlcat(yp, y, sizeof(yp));
    create_directory_if_not_exists(yp);

    /* ay klasörü */
    strlcpy(mp, yp, sizeof(mp));
    strlcat(mp, "/", sizeof(mp));
    strlcat(mp, m, sizeof(mp));
    create_directory_if_not_exists(mp);

    /* gün klasörü */
    strlcpy(dp, mp, sizeof(dp));
    strlcat(dp, "/", sizeof(dp));
    strlcat(dp, d, sizeof(dp));
    create_directory_if_not_exists(dp);

    /* tam dosya yolu */
    snprintf(out_path, cap, "%s/%s-%s-%s_%s-%s-%s.csv",
             dp, y, m, d, H, M, S);

    return ESP_OK;
}


esp_err_t storage_write_sensor_data(const char *timestamp,
                                    const char *label,
                                    float value,
                                    const char *unit)
{
    if (!s_sd_mounted) return ESP_ERR_INVALID_STATE;

    char path[256];
    build_sensor_csv_path(timestamp, path, sizeof(path));

    FILE *f = fopen(path, "a");
    if (!f) return ESP_FAIL;

    fprintf(f, "%s,%s,%.4f,%s\n",
            timestamp, label, value, unit ? unit : "");
    fclose(f);

    return ESP_OK;
}

esp_err_t storage_write_data(const char *timestamp, float avg)
{
    return storage_write_sensor_data(timestamp, "Average", avg, "");
}

/* ------------------- FRAME YAZ ------------------- */
esp_err_t storage_write_frame(const char *frame)
{
    if (!s_sd_mounted) return ESP_ERR_INVALID_STATE;
    if (!frame || strlen(frame) == 0) return ESP_ERR_INVALID_ARG;

    char date[16], time[16];
    time_if_get_date(date, sizeof(date));   // DD/MM/YYYY
    time_if_get_time(time, sizeof(time));   // HH:MM:SS

    int day, mon, yr;
    int H, M, S;

    sscanf(date, "%2d/%2d/%4d", &day, &mon, &yr);
    sscanf(time, "%2d:%2d:%2d", &H, &M, &S);

    char dir[256];
    snprintf(dir, sizeof(dir),
             "%s/%04d/%02d/%02d",
             SD_MOUNT_POINT, yr, mon, day);

    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s/%04d", SD_MOUNT_POINT, yr); mkdir(tmp, 0755);
    snprintf(tmp, sizeof(tmp), "%s/%04d/%02d", SD_MOUNT_POINT, yr, mon); mkdir(tmp, 0755);
    snprintf(tmp, sizeof(tmp), "%s/%04d/%02d/%02d", SD_MOUNT_POINT, yr, mon, day); mkdir(tmp, 0755);

    char filepath[256];
    /* filepath oluşturma – snprintf yerine güvenli strlcpy/strlcat */
    strlcpy(filepath, dir, sizeof(filepath));
    strlcat(filepath, "/", sizeof(filepath));

    char fname[32];
    snprintf(fname, sizeof(fname), "%02d-%02d-%02d.log", H, M, S);

    strlcat(filepath, fname, sizeof(filepath));

    FILE *f = fopen(filepath, "a");
    if (!f) return ESP_FAIL;

    fwrite(frame, 1, strlen(frame), f);
    fclose(f);

    return ESP_OK;
}

/* ------------------- MANUEL YOL ------------------- */
esp_err_t storage_prepare_paths_manual(int y, int m, int d, int H,
                                       char *out_dir, size_t out_dir_cap,
                                       char *out_file, size_t out_file_cap)
{
    snprintf(out_dir, out_dir_cap,
             "%s/%04d/%02d/%02d",
             SD_MOUNT_POINT, y, m, d);

    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s/%04d", SD_MOUNT_POINT, y); mkdir(tmp, 0755);
    snprintf(tmp, sizeof(tmp), "%s/%04d/%02d", SD_MOUNT_POINT, y, m); mkdir(tmp, 0755);
    snprintf(tmp, sizeof(tmp), "%s/%04d/%02d/%02d", SD_MOUNT_POINT, y, m, d); mkdir(tmp, 0755);

    snprintf(out_file, out_file_cap,
             "%s/%02d-00-00.log",
             out_dir, H);

    return ESP_OK;
}

/* ------------------- DURUM ------------------- */
bool storage_is_available(void)
{
    return s_sd_mounted;
}
