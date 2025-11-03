#include "cfg_if.h"
#include "esp_log.h"
// Diğer NVS kütüphaneleri buraya dahil edilecek

static const char *TAG = "CFG_NVS";

esp_err_t cfg_init(void) {
    ESP_LOGI(TAG, "CFG_IF bileşeni başlatılıyor (NVS).");
    // Burada NVS başlatma ve varsayılan ayarları yükleme kodu olacak.
    
    // Örnek: nvs_flash_init() çağrısı buraya gelir.
    
    return ESP_OK; // Başarılı ise döndür
}