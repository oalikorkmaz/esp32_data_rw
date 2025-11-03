#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
// Tüm bileşen arayüzlerini dahil et
#include "cfg_if.h"
#include "storage_if.h"
#include "time_if.h"
#include "net_if.h"
#include "parser_hex.h"
#include "ble_system_if.h"
#include "nvs_flash.h"
#include "include/state_machine.h"
#include "ble_led_if.h"

static const char *TAG = "APP_MAIN";

// ----------------------------------------------------
// FreeRTOS Görev Tanımları
// ----------------------------------------------------

void data_collector_task(void *pvParameters) {
    ESP_LOGI(TAG, "Data Collector Görevi Başlatıldı.");
    while (1) {
        // ... Veri toplama mantığı ...
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void comm_manager_task(void *pvParameters) {
    ESP_LOGI(TAG, "Haberleşme Yöneticisi Görevi Başlatıldı.");
    while (1) {
        // ... Haberleşme mantığı ...
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}
void app_main(void) {
    ESP_LOGI(TAG, "--- Sistem Başlatılıyor: Donanım/Yazılım Init Evresi ---");

    ble_led_init();
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(ble_system_init());

    // 1. Temel Tek Seferlik Başlatmalar
    //ESP_ERROR_CHECK(cfg_init());       // NVS
    //ESP_ERROR_CHECK(storage_init());   // SD/SPIFFS
    //ESP_ERROR_CHECK(net_init());       // Ağ Yığını
    //ESP_ERROR_CHECK(time_init());      // RTC/SNTP
    //ESP_ERROR_CHECK(parser_init());    // Veri İşleyici init
    
    // 2. Etkileşimli Servisler
    // Bluetooth başlatma ve reklam ayarları bu fonksiyonun içinde.
    
    ESP_LOGI(TAG, "--- Tüm Temel Bileşenler Başlatıldı. Görev Oluşturma Başlıyor ---");

    // 3. FreeRTOS Görevlerinin Oluşturulması
    // ESP_ERROR_CHECK(state_machine_init());
    
    // xTaskCreate(data_collector_task, 
    //             "DATA_COLL", 
    //             4096, 
    //             NULL, 
    //             configMAX_PRIORITIES - 3, 
    //             NULL);
    
    // xTaskCreate(comm_manager_task, 
    //             "COMM_MGR", 
    //             6144, 
    //             NULL, 
    //             configMAX_PRIORITIES - 4, 
    //             NULL);
    
    ESP_LOGI(TAG, "--- FreeRTOS Görevleri Başarıyla Oluşturuldu. Sistem Hazır. ---");
}