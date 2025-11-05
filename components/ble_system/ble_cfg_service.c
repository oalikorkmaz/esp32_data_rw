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
#include "net_manager.h"   // 🔹 ağ modu seçimi
#include "nvs_flash.h"
#include "nvs.h"
#include "string.h"


static const char *TAG = "BLE_CFG";
#define DEVICE_NAME "ESP CFG"
static uint8_t ble_addr_type;

/* ------------------------------------------------------------
 * BLE GAP / Advertising
 * ------------------------------------------------------------ */
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
        ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER,
                          &adv_params, ble_gap_event, NULL);
        break;

    default:
        break;
    }
    return 0;
}

/* ------------------------------------------------------------
 * BLE INIT
 * ------------------------------------------------------------ */
esp_err_t ble_cfg_init(void)
{
    ESP_LOGI(TAG, "BLE CFG yapılandırma hazır (henüz servis başlatılmadı).");
    return ESP_OK;
}

static void ble_on_sync(void)
{
    ble_hs_id_infer_auto(0, &ble_addr_type);
    ble_svc_gap_device_name_set(DEVICE_NAME);

    struct ble_hs_adv_fields fields = {0};
    fields.name = (uint8_t *)DEVICE_NAME;
    fields.name_len = strlen(DEVICE_NAME);
    fields.name_is_complete = 1;
    ble_gap_adv_set_fields(&fields);

    ESP_LOGI(TAG, "BLE advertising başladı. Cihaz adı: %s", DEVICE_NAME);
    ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER,
                      &adv_params, ble_gap_event, NULL);
}

/* ------------------------------------------------------------
 * BLE GATT SERVICE (configuration characteristic)
 * ------------------------------------------------------------ */
static int cfg_write_cb(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR)
        return 0;

    char buf[64] = {0};
    int len = OS_MBUF_PKTLEN(ctxt->om);
    os_mbuf_copydata(ctxt->om, 0, len, buf);
    buf[len] = '\0';
    ESP_LOGI(TAG, "BLE'den gelen veri: %s", buf);

    /* ------------------------------------------------------------
     * 1️⃣ Wi-Fi yapılandırma verisi (örnek: wifi:SSID,PASS)
     * ------------------------------------------------------------ */
    if (strncmp(buf, "wifi:", 5) == 0) {
        char *ssid = strtok(buf + 5, ",");
        char *pass = strtok(NULL, ",");
        if (ssid && pass) {
            // NVS’ye kaydet
            nvs_handle_t handle;
            if (nvs_open("wifi_cfg", NVS_READWRITE, &handle) == ESP_OK) {
                nvs_set_str(handle, "ssid", ssid);
                nvs_set_str(handle, "pass", pass);
                nvs_commit(handle);
                nvs_close(handle);
                ESP_LOGI(TAG, "Wi-Fi bilgileri kaydedildi → SSID: %s", ssid);
            } else {
                ESP_LOGE(TAG, "Wi-Fi bilgileri kaydedilemedi (NVS hatası).");
            }
        } else {
            ESP_LOGW(TAG, "Wi-Fi format hatalı! Beklenen: wifi:SSID,PASS");
        }
        return 0;
    }

    /* ------------------------------------------------------------
     * 2️⃣ Ağ modu seçimi ("0"=Ethernet, "1"=Wi-Fi, "2"=GSM)
     * ------------------------------------------------------------ */
    int mode = atoi(buf);
    if (mode >= 0 && mode <= 2) {
        net_manager_set_mode((net_mode_t)mode);
        ESP_LOGI(TAG, "Ağ modu değiştirildi -> %d", mode);
    } else {
        ESP_LOGW(TAG, "Geçersiz ağ modu: %s", buf);
    }

    return 0;
}


/* UUID'ler ve servis tablosu */
static const ble_uuid128_t gatt_svc_uuid =
    BLE_UUID128_INIT(0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF,
                     0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF);

static const ble_uuid128_t cfg_char_uuid =
    BLE_UUID128_INIT(0xAB, 0xCD, 0xEF, 0x01, 0x02, 0x03, 0x04, 0x05,
                     0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D);

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &cfg_char_uuid.u,
                .access_cb = cfg_write_cb,
                .flags = BLE_GATT_CHR_F_WRITE,
            },
            {0},
        },
    },
    {0},
};

/* ------------------------------------------------------------
 * BLE Host Task
 * ------------------------------------------------------------ */
static void ble_host_task(void *param)
{
    ESP_LOGI(TAG, "NimBLE host task çalışıyor...");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/* ------------------------------------------------------------
 * BLE START
 * ------------------------------------------------------------ */
esp_err_t ble_cfg_start(void)
{
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_gatts_count_cfg(gatt_svcs);
    ble_gatts_add_svcs(gatt_svcs);

    ble_hs_cfg.sync_cb = ble_on_sync;
    nimble_port_freertos_init(ble_host_task);

    ESP_LOGI(TAG, "BLE CFG Servisi başarıyla başlatıldı.");
    return ESP_OK;
}
