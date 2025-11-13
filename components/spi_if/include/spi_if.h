#ifndef SPI_IF_H
#define SPI_IF_H

#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

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

typedef void *spi_if_device_handle_t;

esp_err_t spi_if_register_device(spi_host_device_t host, gpio_num_t cs_io, spi_if_device_handle_t *out_handle);
void spi_if_unregister_device(spi_if_device_handle_t handle);
esp_err_t spi_if_bus_lock_acquire(spi_if_device_handle_t handle, TickType_t ticks_to_wait);
void spi_if_bus_lock_release(spi_if_device_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // SPI_IF_H
