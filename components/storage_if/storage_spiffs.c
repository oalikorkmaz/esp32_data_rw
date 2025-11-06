#include "storage_if.h"
#include "esp_log.h"
// Diğer SPIFFS/SD kütüphaneleri buraya dahil edilecek

static const char *TAG = "STORAGE_SPIFFS";

esp_err_t storage_init(void) {
    ESP_LOGI(TAG, "STORAGE_IF bileşeni başlatılıyor (SPIFFS).");
    // Burada SPIFFS veya SD kartın mount etme kodu olacak.
    
    return ESP_OK;
}

esp_err_t storage_write_data(const char *timestamp, float avg)
{
    char line[64];
    snprintf(line, sizeof(line), "%s, %.2f\n", timestamp, avg);
    return storage_write_file("/data.csv", line, strlen(line), true);
}
