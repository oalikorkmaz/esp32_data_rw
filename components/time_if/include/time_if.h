#ifndef TIME_IF_H
#define TIME_IF_H

#include "esp_err.h"
#include <time.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------------------
// Zaman Yönetimi Operasyonları (RTC veya SNTP için ortak)
// ----------------------------------------------------

/**
 * @brief Zaman arayüzünü (SNTP/DS1302) başlatır.
 * @return esp_err_t ESP_OK veya hata kodu.
 */
esp_err_t time_if_init(void);

/**
 * @brief Cihazın mevcut saatini ve tarihini alır.
 * @param current_time struct tm formatında zaman bilgisini döndüren pointer.
 * @return esp_err_t ESP_OK veya hata kodu.
 */
esp_err_t time_get_datetime(struct tm *current_time);

/**
 * @brief Unix zaman damgasını (epoch time) döndürür.
 * @return time_t Geçerli Unix zaman damgası.
 */
time_t time_get_epoch(void);

/**
 * @brief Zamanın (SNTP/RTC) senkronize olup olmadığını kontrol eder.
 * @return bool Senkronize ise true, değilse false.
 */
bool time_is_synchronized(void);
void time_if_get_formatted_timestamp(char *buffer, size_t len);
void time_if_get_date(char *buffer, size_t len);
void time_if_get_time(char *buffer, size_t len);



#ifdef __cplusplus
}
#endif

#endif // TIME_IF_H