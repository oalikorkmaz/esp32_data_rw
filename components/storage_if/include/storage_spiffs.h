#ifndef STORAGE_SPIFFS_H
#define STORAGE_SPIFFS_H


#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------------------
// Dosya Sistemi Operasyonları (SD Kart)
// ----------------------------------------------------

/**
 * @brief SD Kartı başlatır ve mount eder.
 * @return esp_err_t ESP_OK veya hata kodu.
 */
esp_err_t storage_init(void);

/**
 * @brief SD Kartı unmount eder.
 * @return esp_err_t ESP_OK veya hata kodu.
 */
esp_err_t storage_deinit(void);

/**
 * @brief Belirtilen dosya yoluna veri yazar.
 * @param path Yazılacak dosyanın yolu (örn: "/data.csv")
 * @param data_buffer Yazılacak veri.
 * @param len Veri uzunluğu.
 * @param append true = dosyanın sonuna ekle, false = üzerine yaz.
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

/**
 * @brief Timestamp ve ortalama değeri CSV formatında yazar.
 * @param timestamp Zaman damgası string'i.
 * @param avg Ortalama değer.
 * @return esp_err_t ESP_OK veya hata kodu.
 */
esp_err_t storage_write_data(const char *timestamp, float avg);

/**
 * @brief Sensör verisini hiyerarşik klasör yapısında kaydeder.
 * 
 * Klasör yapısı: /sdcard/YYYY/MM/DD/YYYY-MM-DD_HH-MM-SS.csv
 * 
 * @param timestamp Zaman damgası ("2024-08-01 12:08:31")
 * @param label Sensör etiketi (örn: "Albedo", "Pyra")
 * @param value Sensör değeri
 * @param unit Ölçüm birimi (örn: "W/m2")
 * @return esp_err_t ESP_OK veya hata kodu.
 */
esp_err_t storage_write_sensor_data(const char *timestamp, const char *label, 
                                     float value, const char *unit);


esp_err_t storage_prepare_paths_manual(int year, int month, int day, int hour,
                                       char *out_date_dir, size_t out_date_dir_cap,
                                       char *out_hour_file, size_t out_hour_file_cap);

/**
 * @brief SD Kart mount edilmiş mi kontrol eder.
 * @return true SD Kart hazır, false SD Kart yok.
 */
bool storage_is_available(void);

#ifdef __cplusplus
}
#endif

#endif // STORAGE_IF_H