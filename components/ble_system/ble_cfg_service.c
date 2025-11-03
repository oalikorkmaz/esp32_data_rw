#include "ble_cfg_if.h"
#include "esp_log.h"
#include "esp_err.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

static const char *TAG = "BLE_CFG";
#define DEVICE_NAME "ESP CFG"
static uint8_t ble_addr_type;

// Reklam parametreleri
struct ble_gap_adv_params adv_params = {
        .conn_mode = BLE_GAP_CONN_MODE_UND,
        .disc_mode = BLE_GAP_DISC_MODE_GEN,
        .itvl_min = 0x00A0, // 100 ms
        .itvl_max = 0x00A0,
};

static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0)
                ESP_LOGI(TAG, "Cihaz bağlandı.");
            else
                ESP_LOGI(TAG, "Bağlantı başarısız; reklam yeniden başlatılıyor...");
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Bağlantı kesildi; reklam yeniden başlatılıyor...");
            ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
            break;

        default:
            break;
    }
    return 0;
}

esp_err_t ble_cfg_init(void)
{
    ESP_LOGI(TAG, "BLE CFG yapılandırma hazır (henüz servis başlatılmadı).");
    return ESP_OK;
}

static void ble_on_sync(void)
{
    ble_hs_id_infer_auto(0, &ble_addr_type);
    ble_svc_gap_device_name_set(DEVICE_NAME);

    // Reklam paketi oluştur
    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));
    fields.name = (uint8_t *)DEVICE_NAME;
    fields.name_len = strlen(DEVICE_NAME);
    fields.name_is_complete = 1;
    ble_gap_adv_set_fields(&fields);



    ESP_LOGI(TAG, "BLE advertising başladı. Cihaz adı: %s", DEVICE_NAME);
    ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
}

// NimBLE host task
static void ble_host_task(void *param)
{
    ESP_LOGI(TAG, "NimBLE host task çalışıyor...");
    nimble_port_run();
    nimble_port_freertos_deinit();
}


esp_err_t ble_cfg_start(void)
{
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_hs_cfg.sync_cb = ble_on_sync;
    nimble_port_freertos_init(ble_host_task);
    ESP_LOGI(TAG, "BLE CFG Servisi başarıyla başlatıldı.");
    return ESP_OK;
}
