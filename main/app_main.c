#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

/* ---- Modül Başlıkları ---- */
#include "cfg_if.h"
#include "storage_spiffs.h"
#include "time_if.h"
#include "data_parser.h"
#include "telemetry_service.h"
#include "data_sender.h"
#include "net_manager.h"
#include "serial_if.h"
#include "ble_system_if.h"


static const char *TAG = "APP_MAIN";



/* ---------------------------- SPIFFS İNİTİALİZASYONU ---------------------------- */
#include "esp_spiffs.h"

static void init_spiffs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE("SPIFFS", "Mount failed: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI("SPIFFS", "SPIFFS mount OK");
}

/* ---------------------------- MANUEL VERİ GÖNDERİM TESTİ ---------------------------- */
static void test_manual_send_task(void *arg)
{
    ESP_LOGI("TEST_MANUAL", "Manuel veri gönderim testi başlıyor...");

    // Elle oluşturulmuş veri kaydı
    hd32mt_data_t record = {0};

    // Elle kanal sayısı ve değerleri
    record.sensor_count = 10;
    record.sensors[0] = 12.34;
    record.sensors[1] = 56.78;
    record.sensors[2] = 90.12;

    // Zamanı elle verelim
    const char *manual_timestamp = "11/11/05-14:52:56";

    // Cihaz ID'si de test amaçlı sabit
    const char *manual_device_id = "00-08-DC-20-00-59";

    // FRAME oluşturma için override (data_sender.c içindeki cfg_get yerine kullanacağız)
    ESP_LOGI("TEST_MANUAL", "Test verileri:");
    ESP_LOGI("TEST_MANUAL", "Device ID  : %s", manual_device_id);
    ESP_LOGI("TEST_MANUAL", "Timestamp  : %s", manual_timestamp);
    ESP_LOGI("TEST_MANUAL", "Channels   : %d", record.sensor_count);
    ESP_LOGI("TEST_MANUAL", "Values     : %.2f, %.2f, %.2f",
             record.sensors[0], record.sensors[1], record.sensors[2]);

    // Burada data_sender fonksiyonuna doğrudan çağrı yapıyoruz
    bool ok = data_sender_send_frame_from_record(&record,
                                                 record.sensor_count,
                                                 NULL);

    ESP_LOGI("TEST_MANUAL", "Gönderim sonucu: %s", ok ? "OK" : "FAIL");

    vTaskDelete(NULL);
}
/* ---------------------------- TEST VERİ PARSİNG VE GÖNDERİMİ ---------------------------- */

static const char *TAG_TEST = "TEST_INJECT";

static void test_inject_task(void *arg)
{
    FILE *f = fopen("/spiffs/DELTA SAMPLE DATA.txt", "r");  // SPIFFS'ten okuyoruz
    if (!f) {
        ESP_LOGE(TAG_TEST, "Dosya açılamadı!");
        vTaskDelete(NULL);
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        // Satırdaki \r\n karakterlerini temizle
        char *p = strchr(line, '\r'); if (p) *p = '\0';
        p = strchr(line, '\n'); if (p) *p = '\0';

        if (strlen(line) < 3) continue;  // boş satır geç

        ESP_LOGI(TAG_TEST, "Injecting: %s", line);

        // 1️⃣ Parser’a gönder
        hd32mt_data_t parsed_record = {0};
        if (parse_hd32mt_record(line, &parsed_record)) {
            ESP_LOGI(TAG_TEST, "Parse OK, %d kanal", parsed_record.sensor_count);

            // 2️⃣ Sender’a gönder
            bool ok = data_sender_send_frame_from_record(&parsed_record,
                                                         parsed_record.sensor_count,
                                                         NULL);
            ESP_LOGI(TAG_TEST, "Send result: %s", ok ? "OK" : "FAIL");
        } else {
            ESP_LOGW(TAG_TEST, "Parse FAIL: %s", line);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));  // her satır arasında 1 sn bekle
    }

    fclose(f);
    ESP_LOGI(TAG_TEST, "Test veri enjeksiyonu tamamlandı.");
    vTaskDelete(NULL);
}


/* ---------------------------- ANA GİRİŞ ---------------------------- */

void app_main(void)
{
    ESP_LOGI(TAG, "==============================");
    ESP_LOGI(TAG, "     Sistem Başlatılıyor      ");
    ESP_LOGI(TAG, "==============================");

    /* 1️⃣ Kalıcı hafıza (NVS) ve cihaz konfigürasyonu */
    ESP_ERROR_CHECK(nvs_flash_init());
    cfg_init();
    const device_cfg_t *cfg = cfg_get();
    ESP_LOGI(TAG, "Cihaz ID: %s", cfg->device_id);
    
    init_spiffs();

    /* 2️⃣ RTC (time_if) */
    ESP_ERROR_CHECK(time_if_init());

    /* 3️⃣ SD kart (storage_if) */
    if (storage_init() != ESP_OK || !storage_is_available()) {
        ESP_LOGW(TAG, "SD kart bulunamadı — sadece ağ gönderimi yapılacak.");
    } else {
        ESP_LOGI(TAG, "SD kart hazır ✅");
    }

    /* 4️⃣ Ağ yöneticisi (Ethernet varsayılan) */
    net_manager_set_mode(NET_MODE_ETHERNET);
    net_manager_create_task();

    ESP_LOGI(TAG, "Ağ bağlantısı bekleniyor...");
    while (!net_manager_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    ESP_LOGI(TAG, "Ağ bağlantısı kuruldu ✅");

    /* 5️⃣ Telemetri servisi */
    if (!telemetry_service_start(/* toplam kanal sayısı */ 10)) {
        ESP_LOGE(TAG, "Telemetri servisi başlatılamadı!");
    } else {
        ESP_LOGI(TAG, "Telemetri servisi başlatıldı ✅");
    }

    /* 6️⃣ BLE */
    ESP_ERROR_CHECK(ble_system_init());


    //xTaskCreate(test_inject_task, "test_inject_task", 4096, NULL, 5, NULL);
    xTaskCreate(test_manual_send_task, "test_manual_send_task", 4096, NULL, 5, NULL);


    ESP_LOGI(TAG, "==============================");
    ESP_LOGI(TAG, "     Sistem Başlatıldı ✅     ");
    ESP_LOGI(TAG, "==============================");

    /* Uygulama sonsuz döngü */
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


