#ifndef ETHERNET_INIT_H
#define ETHERNET_INIT_H

#include "esp_err.h"
/* Pin ayarları */
#define PIN_MISO  13
#define PIN_MOSI  11
#define PIN_SCLK  12
#define PIN_CS    10
#define PIN_RST   8
#define PIN_INT   9
#define SPI_HOST  SPI3_HOST
#define SPI_CLOCK_MHZ 8

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t start_w5500_ethernet(void);

#ifdef __cplusplus
}
#endif

#endif
