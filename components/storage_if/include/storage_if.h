#ifndef STORAGE_IF_H
#define STORAGE_IF_H

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------------------
// Dosya Sistemi Operasyonları (SD Kart veya SPIFFS için ortak)
// ----------------------------------------------------

/**
 * @brief Depolama arayüzünü (SPIFFS/SD Kart) başlatır ve mount eder.
 * @return esp_err_t ESP_OK veya hata kodu.
 */
esp_err_t storage_init(void);

/**
 * @brief Belirtilen dosya yoluna veri yazar.
 * @param path Yazılacak dosyanın yolu.
 * @param data_buffer Yazılacak veri.
 * @param len Veri uzunluğu.
 * @param append Eğer true ise veriyi dosyanın sonuna ekler, false ise dosyayı silip yeniden yazar.
 * @return esp_err_t ESP_OK veya hata kodu.
 */
esp_err_t storage_write_file(const char *path, const void *data_buffer, size_t len, bool append);

/**
 * @brief Belirtilen dosya yolundan veri okur.
 * @param path Okunacak dosyanın yolu.
 * @param read_buffer Verinin okunacağı tampon.
 * @param max_len Okunabilecek maksimum uzunluk.
 * @return int Okunan bayt sayısı, hata durumunda -1.
 */
int storage_read_file(const char *path, void *read_buffer, size_t max_len);

// Diğer SD kart/depolama fonksiyonları buraya eklenebilir.
esp_err_t storage_write_data(const char *timestamp, float avg);


#ifdef __cplusplus
}
#endif

#endif // STORAGE_IF_H