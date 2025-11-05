#include "wifi_init.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "net_manager.h"

#define WIFI_MAX_FAILS 5


static const char *TAG = "WIFI_INIT";
static bool s_wifi_connected = false;
static bool wifi_initialized = false;
static int s_wifi_fail_count = 0;

/* ---------------- Olay işleyicisi ---------------- */
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
                s_wifi_connected = false;
                s_wifi_fail_count++;
                ESP_LOGW(TAG, "Wi-Fi bağlantısı koptu (%d/%d)", s_wifi_fail_count, WIFI_MAX_FAILS);
                net_manager_on_wifi_event(false);

                if (s_wifi_fail_count >= WIFI_MAX_FAILS) {
                    ESP_LOGE(TAG, "Wi-Fi başarısız, bağlantı kapatılıyor!");
                    esp_wifi_stop();          // ❗ IP'yi sıfırlar
                    s_wifi_fail_count = 0;    // sayaç reset
                } else {
                    esp_wifi_connect();       // tekrar dene
                }
                break;

            default:
                break;
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "IP alındı: " IPSTR, IP2STR(&event->ip_info.ip));
        s_wifi_connected = true;
        net_manager_on_wifi_event(true);
    }
}

/* ---------------- Wi-Fi bilgilerini oku ---------------- */
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

/* ---------------- Wi-Fi başlatma ---------------- */
esp_err_t start_wifi_station(void)
{
    if (wifi_initialized) {
        ESP_LOGW(TAG, "Wi-Fi zaten başlatılmış, yeniden init atlanıyor.");
        esp_wifi_disconnect();
        esp_wifi_connect();
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Wi-Fi başlatılıyor (Station modu)...");

    /* NVS ve Netif */
    ESP_ERROR_CHECK(nvs_flash_init());

    /* Netif */
    esp_netif_create_default_wifi_sta();

    /* Wi-Fi stack init */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* NVS’ten kayıtlı SSID/şifre */
    char ssid[32] = {0}, pass[64] = {0};
    if (!load_wifi_credentials(ssid, sizeof(ssid), pass, sizeof(pass))) {
        ESP_LOGW(TAG, "Wi-Fi bilgisi bulunamadı (BLE üzerinden girilmeli).");
        return ESP_ERR_NOT_FOUND;
    }

    wifi_config_t wifi_cfg = {0};
    strncpy((char *) wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid));
    strncpy((char *) wifi_cfg.sta.password, pass, sizeof(wifi_cfg.sta.password));

    /* Event handler kayıtları */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                    ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                    IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    /* Wi-Fi mod ve başlatma */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi ağa bağlanıyor: %s", ssid);
    wifi_initialized = true;
    return ESP_OK;
}

/* ---------------- Bağlantı durumu ---------------- */
bool wifi_is_connected(void)
{
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) != ESP_OK || mode != WIFI_MODE_STA)
        return false;

    // Bağlantı aktif mi?
    wifi_ap_record_t info;
    if (esp_wifi_sta_get_ap_info(&info) == ESP_OK) {
        // RSSI veya channel > 0 ise bağlı
        if (info.primary != 0)
            return true;
    }

    // IP halen mevcut mu?
    esp_netif_t *wifi_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (wifi_netif) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(wifi_netif, &ip_info) == ESP_OK) {
            if (ip_info.ip.addr != 0 && s_wifi_connected)
                return true;
        }
    }

    return false;
}



/* ---------------- Bağlantıyı kes ---------------- */
void stop_wifi_station(void)
{
    if (!wifi_initialized) {
        ESP_LOGW(TAG, "Wi-Fi zaten durdurulmuş veya hiç başlatılmamış.");
        return;
    }

    ESP_LOGI(TAG, "Wi-Fi bağlantısı durduruluyor...");

    esp_err_t err;

    /* 1️⃣ Wi-Fi’yi durdur */
    err = esp_wifi_stop();
    if (err == ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGW(TAG, "Wi-Fi zaten durdurulmuş.");
    } else {
        ESP_ERROR_CHECK(err);
    }

    /* 2️⃣ Event handler’ları kaldır */
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler));

    /* 3️⃣ Wi-Fi sürücüsünü kapat */
    ESP_ERROR_CHECK(esp_wifi_deinit());

    /* 4️⃣ Netif’i yok et */
    esp_netif_t *wifi_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (wifi_netif) {
        ESP_LOGI(TAG, "Wi-Fi netif yok ediliyor...");
        esp_netif_destroy(wifi_netif);
    }

    wifi_initialized = false;
    s_wifi_connected = false;

    ESP_LOGI(TAG, "Wi-Fi bağlantısı tamamen kapatıldı.");
}
