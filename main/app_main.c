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
#include "telemetry_service.h"
#include "data_sender.h"


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
    // SD’yi başlat
    if (storage_init() != ESP_OK || !storage_is_available()) {
        ESP_LOGW("APP_MAIN", "SD yok → internete gonderime DEVAM, yerel arşiv YOK.");
    } else {
        // RTC yok: manuel tarih-saat
        int y = 2025, m = 11, d = 10, h = 17;

        char date_dir[128];
        char hour_file[160];
        if (storage_prepare_paths_manual(y, m, d, h, date_dir, sizeof(date_dir),
                                        hour_file, sizeof(hour_file)) == ESP_OK) {
            ESP_LOGI("APP_MAIN", "Klasörler hazır: %s | Saat dosyası: %s", date_dir, hour_file);

            // İLERDE: her gönderdiğin $...$ satırını hour_file’a da ekle:
            // const char *line = "$00-08-dc-20-00-59$...$\r\n";
            // storage_write_file(hour_file + strlen("/sdcard"), line, strlen(line), true);
            // (storage_write_file iç path'e SD_MOUNT_POINT ekliyor; istersen
            //  oraya küçük bir 'write_raw_line_to_sd(const char*)' helper da ekleriz.)
        }
    }

    
    /* 3. Haberleşme Katmanı */
    ESP_ERROR_CHECK(nvs_flash_init());
    net_manager_set_mode(NET_MODE_ETHERNET);  // Varsayılan Ethernet
    net_manager_create_task();

    /* 4. BLE */
    ESP_ERROR_CHECK(ble_system_init());

    ESP_LOGI(TAG, "Ağ bağlantısı bekleniyor...");
    while (!net_manager_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    ESP_LOGI(TAG, "Ağ bağlı ✅");
    
    // 5) Telemetri hattini tek komutla baslat (10 kanal ornek)
    bool ok = telemetry_service_start(/*total_channel_count=*/10);
    if (!ok) {
        ESP_LOGE(TAG, "Telemetri servisi baslamadi!");
    }
    /* 6. Uygulama Servisleri ve Durum Makinesi */
    ESP_ERROR_CHECK(state_machine_init());

    
    /* 7. FreeRTOS Görevleri */
    xTaskCreate(data_collector_task, "DATA_COLL", 4096, NULL, 5, NULL);
    xTaskCreate(comm_manager_task, "COMM_MGR", 6144, NULL, 4, NULL);


    // --- SADECE TEST İÇİN: device_id'yi elden ver ---
    data_sender_set_device_id_override("00-08-DC-20-00-59");

    // Tek atımlık test çerçevesi:
    //float ch[10] = {0};
    //ch[0] = 12.34f; // ch1
    //ch[1] = 23.45f; // ch2
    //ch[2] = 34.56f; // ch3
    //ch[3] = 45.67f; // ch4
    //ch[4] = 56.78f; // ch5
    //ch[5] = 67.89f; // ch6
    //ch[6] = 10.00f; // ch7
    //ch[7] = 20.00f; // ch8
    //ch[8] = 30.00f; // ch9
    //ch[9] = 40.00f; // ch10

    //char ts[24];
    //snprintf(ts, sizeof ts, "10/11/05-16:06:50"); // "yy/mm/dd-HH:MM:SS"

    

    //telemetry_send_test_frame(10, ts, ch, 10);

    ESP_LOGI(TAG, "--- Sistem Başlatıldı ve Görevler Çalışıyor ---");
}