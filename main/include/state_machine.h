#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------------------
// Durum Tanımları (States)
// ----------------------------------------------------

/**
 * @brief Cihazın mevcut çalışma durumları.
 */
typedef enum {
    STATE_INIT = 0,             // Tüm bileşenlerin başlatıldığı ilk durum
    STATE_CONFIG_WAIT,          // BLE üzerinden ayar bekleniyor
    STATE_NET_CONNECTING,       // Ağ bağlantısı kuruluyor (WiFi/ETH/GSM)
    STATE_OTA_CHECK,            // Kalkışta veya manuel olarak OTA güncellemesi kontrol ediliyor
    STATE_DATA_COLLECTING,      // Veri toplama ve SD karta kayıt aktif
    STATE_DATA_TRANSMITTING,    // Veri sunucuya gönderiliyor
    STATE_ERROR                 // Ciddi bir hata oluştu (Örn: SD Kart Hatası)
} system_state_t;

// ----------------------------------------------------
// Arayüz Fonksiyon Prototipleri
// ----------------------------------------------------

/**
 * @brief Durum makinesi görevini (Task) başlatır.
 * Bu görev, sistemin durum geçişlerini ve ana mantığını yönetir.
 * @return esp_err_t ESP_OK veya hata kodu.
 */
esp_err_t state_machine_init(void);

/**
 * @brief Durum makinesi FreeRTOS görevinin ana döngüsü.
 * Bu fonksiyon doğrudan çağrılmaz, FreeRTOS tarafından Task olarak çalıştırılır.
 * @param pvParameters Kullanılmayan parametre.
 */
void state_machine_task(void *pvParameters);

/**
 * @brief Sistem durumunu dışarıdan ayarlamak için kullanılır (Örn: BLE'den ayar sonrası).
 * @param new_state Yeni durum.
 */
void state_machine_set_state(system_state_t new_state);

#ifdef __cplusplus
}
#endif

#endif // STATE_MACHINE_H