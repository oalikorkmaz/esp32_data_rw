#ifndef SPI_IF_H
#define SPI_IF_H

#include "driver/spi_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SPI bus başlatıcı (tek seferlik)
 *
 * Her SPI host (SPI2_HOST, SPI3_HOST vs.) için sadece bir kez çağrılır.
 * Aynı host tekrar initialize edilirse ESP_OK döner, hata vermez.
 */
esp_err_t spi_if_init(spi_host_device_t host, const spi_bus_config_t *buscfg);

#ifdef __cplusplus
}
#endif

#endif // SPI_IF_H
