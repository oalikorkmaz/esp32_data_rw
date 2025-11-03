// Dosya: main/state_machine.c

#include "include/state_machine.h"
#include "esp_log.h"
#include "freertos/task.h"

// Diğer bileşen header'ları burada dahil edilebilir (Örn: net_if.h)

static const char *TAG = "STATE_MACHINE";

// Mevcut durumu takip eden değişken
static system_state_t current_state = STATE_INIT; 

/**
 * @brief Durum makinesi görevini (Task) başlatır.
 */
esp_err_t state_machine_init(void) {
    ESP_LOGI(TAG, "Durum Makinesi Başlatma Fonksiyonu Çalıştı.");

    // Durum makinesi görevini FreeRTOS ile başlat
    BaseType_t result = xTaskCreate(state_machine_task, 
                                    "STATE_MACH", 
                                    4096,         // Stack boyutu
                                    NULL,         // Parametre yok
                                    tskIDLE_PRIORITY + 2, // Öncelik (main'den biraz yüksek)
                                    NULL);        // Handle gerekmiyor

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Durum Makinesi görevi oluşturulamadı!");
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * @brief Sistem durumunu dışarıdan ayarlamak için kullanılır.
 */
void state_machine_set_state(system_state_t new_state) {
    if (new_state != current_state) {
        ESP_LOGW(TAG, "Durum Değişimi: %d -> %d", current_state, new_state);
        current_state = new_state;
        // Burada FreeRTOS Event Group veya Semaphore ile Task'ı uyandırma mekanizması olabilir.
    }
}

/**
 * @brief Durum makinesi FreeRTOS görevinin ana döngüsü.
 */
void state_machine_task(void *pvParameters) {
    ESP_LOGI(TAG, "Durum Makinesi Görevi Başladı.");
    
    // Başlangıçta konfigürasyon bekleniyor durumuna geç
    state_machine_set_state(STATE_CONFIG_WAIT); 

    while (1) {
        switch (current_state) {
            case STATE_CONFIG_WAIT:
                // BLE'den ayar yapıldı mı kontrol et.
                // Eğer ayarlar tamamlandıysa:
                // state_machine_set_state(STATE_NET_CONNECTING);
                break;

            case STATE_NET_CONNECTING:
                // net_if modülünü kullanarak bağlantıyı başlat.
                // Bağlantı kurulursa: 
                // state_machine_set_state(STATE_OTA_CHECK);
                break;

            case STATE_OTA_CHECK:
                // Firmware_Updater modülünü kullanarak versiyon kontrolü yap.
                // Güncelleme yoksa:
                // state_machine_set_state(STATE_DATA_COLLECTING);
                break;
            
            case STATE_DATA_COLLECTING:
                // Veri toplama ve kaydetme görevlerini izle.
                // Veri gönderme periyodu geldiyse:
                // state_machine_set_state(STATE_DATA_TRANSMITTING);
                break;

            case STATE_DATA_TRANSMITTING:
                // comm_manager'a veri gönderme sinyali gönder.
                // Gönderim başarılı/başarısız olursa:
                // state_machine_set_state(STATE_DATA_COLLECTING);
                break;

            case STATE_ERROR:
                // Hata durumunu logla ve kurtarma denemesi yap veya askıda kal.
                break;
            
            default:
                // Bilinmeyen durum
                break;
        }

        // Durum makinesi döngüsünün yavaş çalışması için kısa bir gecikme
        vTaskDelay(pdMS_TO_TICKS(100)); 
    }
}