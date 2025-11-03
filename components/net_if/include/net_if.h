#ifndef NET_IF_H
#define NET_IF_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------------------
// 1. Haberleşme Modu Tanımı
// ----------------------------------------------------

/**
 * @brief Cihazın kullanabileceği haberleşme modları.
 */
typedef enum {
    NET_MODE_NONE = 0,
    NET_MODE_WIFI = 1,
    NET_MODE_ETHERNET = 2,
    NET_MODE_GSM = 3
} net_mode_t;

// ----------------------------------------------------
// 2. Arayüz Fonksiyon Prototipleri
// ----------------------------------------------------

/**
 * @brief Ağ arayüzü bileşenini başlatır.
 * Bu, temel TCP/IP yığınını ve event döngüsünü başlatır.
 * @return esp_err_t ESP_OK veya hata kodu.
 */
esp_err_t net_init(void);

/**
 * @brief Kullanılacak aktif haberleşme modunu ayarlar ve başlatır/durdurur.
 * @param mode Yeni haberleşme modu (NET_MODE_WIFI, NET_MODE_GSM, vb.)
 * @return esp_err_t Başarılı ise ESP_OK.
 */
esp_err_t net_set_mode(net_mode_t mode);

/**
 * @brief Şu anda aktif olan haberleşme modunu döndürür.
 */
net_mode_t net_get_current_mode(void);

/**
 * @brief Aktif arayüzün (WiFi, ETH, GSM) server'a bağlı olup olmadığını kontrol eder.
 * @return bool Bağlı ise true, değilse false.
 */
bool net_is_connected(void);

/**
 * @brief Veriyi server'a gönderir (Protokol bağımsız).
 * Bu fonksiyon, dahili olarak seçilen modu kullanır.
 * @param data_buffer Gönderilecek veri.
 * @param data_len Gönderilecek verinin uzunluğu.
 * @return esp_err_t ESP_OK veya haberleşme hatası.
 */
esp_err_t net_send_data(const uint8_t *data_buffer, size_t data_len);

// ----------------------------------------------------
// 3. Event/Durum Kontrolü
// ----------------------------------------------------

/**
 * @brief Haberleşme durumundaki önemli değişiklikleri (Bağlandı/Bağlantı Kesildi)
 * diğer görevlere bildirmek için kullanılacak Event Group handle'ı.
 */
// extern EventGroupHandle_t net_event_group; 
// (Bu, FreeRTOS iletişimi için gereklidir, sonraki adımda detaylandıracağız.)

#ifdef __cplusplus
}
#endif

#endif // NET_IF_H