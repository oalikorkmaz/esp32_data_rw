#include "wifi_init.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "net_manager.h"

static const char *TAG = "WIFI_INIT";
static bool s_wifi_connected = false;

/* Olay işleyicisi */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "Wi-Fi başlatıldı, bağlantı deneniyor...");
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGW(TAG, "Wi-Fi bağlantısı koptu, yeniden deneniyor...");
                s_wifi_connected = false;
                esp_wifi_connect();
                break;
            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "IP alındı: " IPSTR, IP2STR(&event->ip_info.ip));
        s_wifi_connected = true;
        net_manager_on_wifi_event(true);
    }
}

/* Kaydedilmiş SSID / parola NVS'den okunur */
static bool load_wifi_credentials(char *ssid, size_t ssid_len,
                                  char *pass, size_t pass_len)
{
    nvs_handle_t handle;
    if (nvs_open("wifi_cfg", NVS_READONLY, &handle) != ESP_OK)
        return false;

    esp_err_t err1 = nvs_get_str(handle, "ssid", ssid, &ssid_len);
    esp_err_t err2 = nvs_get_str(handle, "pass", pass, &pass_len);
    nvs_close(handle);

    if (err1 == ESP_OK && err2 == ESP_OK) {
        ESP_LOGI(TAG, "Kaydedilen Wi-Fi: %s", ssid);
        return true;
    }
    return false;
}

/* Wi-Fi başlatma */
esp_err_t start_wifi_station(void)
{
    ESP_LOGI(TAG, "Wi-Fi başlatılıyor (Station modu)...");

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    esp_event_loop_create_default();

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    char ssid[32] = {0}, pass[64] = {0};
    if (!load_wifi_credentials(ssid, sizeof(ssid), pass, sizeof(pass))) {
        ESP_LOGW(TAG, "Wi-Fi bilgisi bulunamadı (BLE üzerinden girilmeli).");
        return ESP_ERR_NOT_FOUND;
    }

    wifi_config_t wifi_cfg = {0};
    strncpy((char *)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid));
    strncpy((char *)wifi_cfg.sta.password, pass, sizeof(wifi_cfg.sta.password));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                    ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                    IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi ağa bağlanıyor: %s", ssid);
    return ESP_OK;
}

/* bağlantı durumu alınabilir */
bool wifi_is_connected(void)
{
    return s_wifi_connected;
}
