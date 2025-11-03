#include "storage_if.h"
#include "esp_log.h"
// Diğer SPIFFS/SD kütüphaneleri buraya dahil edilecek

static const char *TAG = "STORAGE_SPIFFS";

esp_err_t storage_init(void) {
    ESP_LOGI(TAG, "STORAGE_IF bileşeni başlatılıyor (SPIFFS).");
    // Burada SPIFFS veya SD kartın mount etme kodu olacak.
    
    return ESP_OK;
}