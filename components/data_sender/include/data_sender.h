#ifndef DATA_SENDER_H
#define DATA_SENDER_H

#include <stdbool.h>

/**
 * @brief Sensör verisini sunucuya gönderir.
 * 
 * @param label     Sensör etiketi (örn. "Albedo", "Pyra")
 * @param value     Sensör değeri
 * @param unit      Ölçüm birimi (örn. "W/m2")
 * @param timestamp Tarih-saat bilgisi ("2024-08-01 12:08:31")
 * 
 * @return true     Başarılı gönderim
 * @return false    Hata oluştu veya bağlantı yok
 */
bool data_sender_send(const char *label, float value, const char *unit, const char *timestamp);

#endif
