#include "spi_if.h"
#include "driver/spi_master.h"
#include "esp_log.h"

static const char *TAG = "SPI_IF";

/*
 * Her SPI host için durum bilgisi.
 * ESP32-S3'te genelde 3 SPI host vardır:
 * SPI1 (flash), SPI2 (HSPI), SPI3 (VSPI)
 */
static bool s_spi_initialized[SOC_SPI_PERIPH_NUM] = { false };

esp_err_t spi_if_init(spi_host_device_t host, const spi_bus_config_t *buscfg)
{
    if (host >= SOC_SPI_PERIPH_NUM) {
        ESP_LOGE(TAG, "Geçersiz SPI host: %d", host);
        return ESP_ERR_INVALID_ARG;
    }

    if (s_spi_initialized[host]) {
        ESP_LOGD(TAG, "SPI%d zaten başlatılmış.", host + 1);
        return ESP_OK;
    }

    esp_err_t ret = spi_bus_initialize(host, buscfg, SPI_DMA_CH_AUTO);
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "SPI%d zaten initialize edilmiş durumda.", host + 1);
        s_spi_initialized[host] = true;
        return ESP_OK;
    }

    ESP_ERROR_CHECK(ret);
    s_spi_initialized[host] = true;
    ESP_LOGI(TAG, "SPI%d başarıyla başlatıldı.", host + 1);
    return ESP_OK;
}
