#ifndef ETHERNET_INIT_H
#define ETHERNET_INIT_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief W5500 Ethernet başlatma fonksiyonu
 *
 * @return
 *      - ESP_OK: Başarılı
 *      - ESP_FAIL: Hata oluştu
 */
esp_err_t start_w5500_ethernet(void);

#ifdef __cplusplus
}
#endif

#endif // ETHERNET_INIT_H
