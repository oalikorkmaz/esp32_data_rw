#ifndef CFG_IF_H
#define CFG_IF_H

#include "esp_err.h" // esp_err_t kullanmak için

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Konfigürasyon Arayüzünü (NVS) başlatır.
 * * @return esp_err_t ESP_OK veya hata kodu
 */
esp_err_t cfg_init(void);

// Diğer cfg_if fonksiyon prototipleri buraya gelecek...

#ifdef __cplusplus
}
#endif

#endif // CFG_IF_H