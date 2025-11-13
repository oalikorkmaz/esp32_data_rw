#include "spi_if.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "esp_log.h"
#include <stdlib.h>

static const char *TAG = "SPI_IF";

/*
 * Her SPI host için durum bilgisi.
 * ESP32-S3'te genelde 3 SPI host vardır:
 * SPI1 (flash), SPI2 (HSPI), SPI3 (VSPI)
 */
typedef struct {
    bool initialized;
    spi_bus_lock_handle_t lock_handle;
} spi_if_bus_ctx_t;

typedef struct {
    spi_host_device_t host;
    gpio_num_t cs_io;
    spi_bus_lock_dev_handle_t lock_dev;
} spi_if_device_ctx_t;

static spi_if_bus_ctx_t s_spi_bus_ctx[SOC_SPI_PERIPH_NUM] = { 0 };

esp_err_t spi_if_init(spi_host_device_t host, const spi_bus_config_t *buscfg)
{
    if (host >= SOC_SPI_PERIPH_NUM) {
        ESP_LOGE(TAG, "Geçersiz SPI host: %d", host);
        return ESP_ERR_INVALID_ARG;
    }

    if (s_spi_bus_ctx[host].initialized) {
        ESP_LOGD(TAG, "SPI%d zaten başlatılmış.", host + 1);
        return ESP_OK;
    }

    esp_err_t ret = spi_bus_initialize(host, buscfg, SPI_DMA_CH_AUTO);
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "SPI%d zaten initialize edilmiş durumda.", host + 1);
    } else {
        ESP_ERROR_CHECK(ret);
    }

    s_spi_bus_ctx[host].initialized = true;

    esp_err_t lock_ret = spi_bus_lock_init(host);
    if (lock_ret != ESP_OK && lock_ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "SPI%d bus lock init hatası: %s", host + 1, esp_err_to_name(lock_ret));
        return lock_ret;
    }
    s_spi_bus_ctx[host].lock_handle = spi_bus_lock_get_by_id(host);
    if (!s_spi_bus_ctx[host].lock_handle) {
        ESP_LOGE(TAG, "SPI%d için bus lock handle alınamadı", host + 1);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "SPI%d başarıyla başlatıldı.", host + 1);
    return ESP_OK;
}

esp_err_t spi_if_register_device(spi_host_device_t host, gpio_num_t cs_io, spi_if_device_handle_t *out_handle)
{
    if (!out_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    if (host >= SOC_SPI_PERIPH_NUM || !s_spi_bus_ctx[host].initialized) {
        ESP_LOGE(TAG, "SPI%d bus henüz başlatılmamış.", host + 1);
        return ESP_ERR_INVALID_STATE;
    }

    spi_if_device_ctx_t *ctx = calloc(1, sizeof(spi_if_device_ctx_t));
    if (!ctx) {
        return ESP_ERR_NO_MEM;
    }

    gpio_config_t cs_cfg = {
        .pin_bit_mask = 1ULL << cs_io,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cs_cfg));
    gpio_set_level(cs_io, 1);

    spi_bus_lock_dev_config_t devcfg = {0};
#ifdef SPI_BUS_LOCK_DEV_FLAG_CS_REQUIRED
    devcfg.flags = SPI_BUS_LOCK_DEV_FLAG_CS_REQUIRED;
#endif

    esp_err_t ret = spi_bus_lock_register_dev(s_spi_bus_ctx[host].lock_handle, &devcfg, &ctx->lock_dev);
    if (ret != ESP_OK) {
        free(ctx);
        return ret;
    }

    ctx->host = host;
    ctx->cs_io = cs_io;
    *out_handle = ctx;
    return ESP_OK;
}

void spi_if_unregister_device(spi_if_device_handle_t handle)
{
    if (!handle) {
        return;
    }

    spi_if_device_ctx_t *ctx = (spi_if_device_ctx_t *)handle;
    if (ctx->lock_dev) {
        spi_bus_lock_unregister_dev(ctx->lock_dev);
    }
    free(ctx);
}

esp_err_t spi_if_bus_lock_acquire(spi_if_device_handle_t handle, TickType_t ticks_to_wait)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    spi_if_device_ctx_t *ctx = (spi_if_device_ctx_t *)handle;
    return spi_bus_lock_acquire_start(ctx->lock_dev, ticks_to_wait);
}

void spi_if_bus_lock_release(spi_if_device_handle_t handle)
{
    if (!handle) {
        return;
    }

    spi_if_device_ctx_t *ctx = (spi_if_device_ctx_t *)handle;
    spi_bus_lock_acquire_end(ctx->lock_dev);
}
