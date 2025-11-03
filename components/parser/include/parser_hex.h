#ifndef PARSER_HEX_H
#define PARSER_HEX_H

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------------------
// Veri Yapıları
// ----------------------------------------------------

// RS232'den alınan ve işlenen verinin yapısı
typedef struct {
    time_t timestamp;        // Veri okunduğunda alınan zaman etiketi
    uint32_t raw_value;      // Ham HEX değeri (veya dönüştürülmüş ilk değer)
    float processed_value;   // Ortalama alma, kalibrasyon vb. sonrası işlenmiş değer
    // ... İleride eklenecek diğer sensör/veri alanları ...
} parsed_data_t;

// ----------------------------------------------------
// Parser Fonksiyon Prototipleri
// ----------------------------------------------------

/**
 * @brief Parser bileşenini başlatır.
 * @return esp_err_t ESP_OK veya hata kodu.
 */
esp_err_t parser_init(void);

/**
 * @brief Ham HEX verisini işler ve anlamlı bir yapıya dönüştürür.
 * @param raw_data RS232'den gelen ham veri tamponu.
 * @param raw_len Ham verinin uzunluğu.
 * @param output_data İşlenmiş verinin yazılacağı çıktı yapısı.
 * @return esp_err_t ESP_OK (başarılı) veya hata kodu (CRC/Protokol Hatası).
 */
esp_err_t parser_process_hex(const uint8_t *raw_data, size_t raw_len, parsed_data_t *output_data);

#ifdef __cplusplus
}
#endif

#endif // PARSER_HEX_H