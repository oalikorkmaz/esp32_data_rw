#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
// Tüm bileşen arayüzlerini dahil et
#include "cfg_if.h"
#include "storage_if.h"
#include "time_if.h"
#include "data_parser.h"
#include "ble_system_if.h"
#include "include/state_machine.h"
#include "ethernet_init.h"
#include "net_manager.h"
#include "serial_if.h"


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
    
    /* 1. Kalıcı Hafıza */
    cfg_init();
    const device_cfg_t *cfg = cfg_get();
    ESP_LOGI("MAIN", "Cihaz ID: %s", cfg->device_id);

    /* 2. Temel Donanımlar */
    ESP_ERROR_CHECK(time_init());              // RTC veya SNTP
    ESP_ERROR_CHECK(storage_init());           // SD kart / SPIFFS
    ESP_LOGI(TAG, "SD Kart veya dosya sistemi hazır.");
    
    /* 3. Haberleşme Katmanı */
    ESP_ERROR_CHECK(nvs_flash_init());
    net_manager_set_mode(NET_MODE_ETHERNET);  // Varsayılan Ethernet
    net_manager_create_task();

    /* 4. BLE / Seri / Parser */
    ESP_ERROR_CHECK(ble_system_init());
    serial_init();

    /* 5. Uygulama Servisleri ve Durum Makinesi */
    ESP_ERROR_CHECK(state_machine_init());

    
    /* 6. FreeRTOS Görevleri */
    xTaskCreate(data_collector_task, "DATA_COLL", 4096, NULL, 5, NULL);
    xTaskCreate(comm_manager_task, "COMM_MGR", 6144, NULL, 4, NULL);

    ESP_LOGI(TAG, "--- Sistem Başlatıldı ve Görevler Çalışıyor ---");
}