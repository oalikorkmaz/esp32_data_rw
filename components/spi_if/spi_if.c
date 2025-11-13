#include "spi_if.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdlib.h>

static const char *TAG = "SPI_IF";

/* Device context yapısı */
typedef struct {
    spi_host_device_t host;
    gpio_num_t cs_io;
    SemaphoreHandle_t mutex;  // Her device'ın kendi mutex'i
} spi_if_device_ctx_t;

/* Bus context yapısı */
typedef struct {
    bool initialized;
    SemaphoreHandle_t bus_mutex;  // Bus seviyesinde mutex
} spi_if_bus_ctx_t;

static spi_if_bus_ctx_t s_spi_bus_ctx[SOC_SPI_PERIPH_NUM] = {0};

/* ============================================
 * SPI BUS INIT
 * ============================================ */
esp_err_t spi_if_init(spi_host_device_t host, const spi_bus_config_t *buscfg)
{
    if (host >= SOC_SPI_PERIPH_NUM) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_spi_bus_ctx[host].initialized) {
        ESP_LOGD(TAG, "SPI%d zaten init edilmiş.", host + 1);
        return ESP_OK;
    }

    // SPI bus'ı başlat
    esp_err_t ret = spi_bus_initialize(host, buscfg, SPI_DMA_CH_AUTO);
    
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "SPI%d daha önce init edilmiş.", host + 1);
        s_spi_bus_ctx[host].initialized = true;
        
        // Bus mutex zaten varsa kullan, yoksa oluştur
        if (!s_spi_bus_ctx[host].bus_mutex) {
            s_spi_bus_ctx[host].bus_mutex = xSemaphoreCreateMutex();
            if (!s_spi_bus_ctx[host].bus_mutex) {
                ESP_LOGE(TAG, "Bus mutex oluşturulamadı!");
                return ESP_ERR_NO_MEM;
            }
        }
        return ESP_OK;
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI%d init hatası: %s", host + 1, esp_err_to_name(ret));
        return ret;
    }

    // Bus mutex oluştur
    s_spi_bus_ctx[host].bus_mutex = xSemaphoreCreateMutex();
    if (!s_spi_bus_ctx[host].bus_mutex) {
        ESP_LOGE(TAG, "Bus mutex oluşturulamadı!");
        spi_bus_free(host);
        return ESP_ERR_NO_MEM;
    }

    s_spi_bus_ctx[host].initialized = true;

    ESP_LOGI(TAG, "✅ SPI%d initialized with mutex lock", host + 1);
    return ESP_OK;
}

/* ============================================
 * DEVICE REGISTER
 * ============================================ */
esp_err_t spi_if_register_device(spi_host_device_t host,
                                 gpio_num_t cs_io,
                                 spi_if_device_handle_t *out_handle)
{
    if (!out_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_spi_bus_ctx[host].initialized) {
        ESP_LOGE(TAG, "SPI%d henüz init edilmemiş!", host + 1);
        return ESP_ERR_INVALID_STATE;
    }

    // Device context oluştur
    spi_if_device_ctx_t *ctx = calloc(1, sizeof(spi_if_device_ctx_t));
    if (!ctx) {
        return ESP_ERR_NO_MEM;
    }

    ctx->host = host;
    ctx->cs_io = cs_io;

    // CS pinini output olarak yapılandır
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << cs_io,
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_down_en = false,
        .pull_up_en = true,  // CS idle HIGH
    };
    gpio_config(&io_conf);
    gpio_set_level(cs_io, 1);  // CS başlangıçta HIGH

    // Device mutex oluştur
    ctx->mutex = xSemaphoreCreateMutex();
    if (!ctx->mutex) {
        free(ctx);
        return ESP_ERR_NO_MEM;
    }

    *out_handle = ctx;

    ESP_LOGI(TAG, "✅ Device registered (Host: SPI%d, CS: GPIO%d)", host + 1, cs_io);
    return ESP_OK;
}

/* ============================================
 * DEVICE UNREGISTER
 * ============================================ */
void spi_if_unregister_device(spi_if_device_handle_t handle)
{
    if (!handle) return;

    spi_if_device_ctx_t *ctx = (spi_if_device_ctx_t *)handle;

    if (ctx->mutex) {
        vSemaphoreDelete(ctx->mutex);
    }

    free(ctx);
    ESP_LOGD(TAG, "Device unregistered");
}

/* ============================================
 * BUS LOCK ACQUIRE
 * ============================================ */
esp_err_t spi_if_bus_lock_acquire(spi_if_device_handle_t handle, TickType_t ticks_to_wait)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    spi_if_device_ctx_t *ctx = (spi_if_device_ctx_t *)handle;

    // Önce bus mutex'ini al
    if (!s_spi_bus_ctx[ctx->host].bus_mutex) {
        ESP_LOGE(TAG, "Bus mutex yok!");
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_spi_bus_ctx[ctx->host].bus_mutex, ticks_to_wait) != pdTRUE) {
        ESP_LOGW(TAG, "Bus mutex alınamadı (timeout)");
        return ESP_ERR_TIMEOUT;
    }

    // Sonra device mutex'ini al
    if (xSemaphoreTake(ctx->mutex, ticks_to_wait) != pdTRUE) {
        // Bus mutex'i geri ver
        xSemaphoreGive(s_spi_bus_ctx[ctx->host].bus_mutex);
        ESP_LOGW(TAG, "Device mutex alınamadı (timeout)");
        return ESP_ERR_TIMEOUT;
    }

    // CS'yi aktif et (LOW)
    gpio_set_level(ctx->cs_io, 0);

    return ESP_OK;
}

/* ============================================
 * BUS LOCK RELEASE
 * ============================================ */
void spi_if_bus_lock_release(spi_if_device_handle_t handle)
{
    if (!handle) return;

    spi_if_device_ctx_t *ctx = (spi_if_device_ctx_t *)handle;

    // CS'yi pasif et (HIGH)
    gpio_set_level(ctx->cs_io, 1);

    // Device mutex'i serbest bırak
    xSemaphoreGive(ctx->mutex);

    // Bus mutex'i serbest bırak
    if (s_spi_bus_ctx[ctx->host].bus_mutex) {
        xSemaphoreGive(s_spi_bus_ctx[ctx->host].bus_mutex);
    }
}