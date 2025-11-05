#include "net_manager.h"
#include "ethernet_init.h"
#include "wifi_init.h"
#include "esp_event.h"
#include "esp_eth.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_ping.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "ping/ping_sock.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

static const char *TAG = "NET_MANAGER";

/* ---- Durum Değişkenleri ---- */
static net_mode_t s_current_mode = NET_MODE_AUTO;
static bool s_wifi_connected = false;
static bool s_eth_link_up = false;
static bool s_event_loop_initialized = false;
static bool s_manual_override = false;   // BLE manuel geçiş
static net_mode_t s_manual_mode = NET_MODE_ETHERNET;

/* ---- Yardımcı Fonksiyonlar ---- */
static void stop_current_connection(void);
static void start_network(void);
static bool ping_test(void);
static bool ethernet_available(void);

/* -------------------------------------------------------
 * Event Callback’ler
 * ------------------------------------------------------- */
void net_manager_on_wifi_event(bool connected)
{
    s_wifi_connected = connected;
    ESP_LOGI(TAG, "Wi-Fi bağlantı durumu: %s", connected ? "UP" : "DOWN");
}

void net_manager_on_eth_event(bool link_up)
{
    s_eth_link_up = link_up;
    ESP_LOGI(TAG, "Ethernet bağlantı durumu: %s", link_up ? "UP" : "DOWN");
}

/* -------------------------------------------------------
 * Manuel Mod (BLE’den gelen)
 * ------------------------------------------------------- */
void net_manager_set_mode(net_mode_t mode)
{
    s_manual_override = true;
    s_manual_mode = mode;
    ESP_LOGW(TAG, "BLE Manuel mod isteği: %d", mode);
}

/* -------------------------------------------------------
 * Event Loop kontrolü
 * ------------------------------------------------------- */
static void ensure_event_loop_initialized(void)
{
    if (!s_event_loop_initialized) {
        ESP_ERROR_CHECK(esp_netif_init());
        esp_err_t ret = esp_event_loop_create_default();
        if (ret == ESP_ERR_INVALID_STATE)
            ESP_LOGW(TAG, "Event loop zaten oluşturulmuş.");
        else
            ESP_ERROR_CHECK(ret);
        s_event_loop_initialized = true;
    }
}

/* -------------------------------------------------------
 * Ethernet var mı kontrolü (SPI üzerinden)
 * ------------------------------------------------------- */
static bool ethernet_available(void)
{
    esp_err_t ret = start_w5500_ethernet();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Ethernet donanımı mevcut.");
        stop_w5500_ethernet(); // sadece varlığını kontrol ettik
        return true;
    }
    ESP_LOGW(TAG, "Ethernet donanımı bulunamadı!");
    return false;
}

/* -------------------------------------------------------
 * Ping testi
 * ------------------------------------------------------- */
static bool ping_test(void)
{
    ip_addr_t target_addr;
    inet_pton(AF_INET, "8.8.8.8", &target_addr);

    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.count = 3;
    ping_config.target_addr = target_addr;

    esp_ping_handle_t ping;
    if (esp_ping_new_session(&ping_config, NULL, &ping) != ESP_OK)
        return false;

    esp_ping_start(ping);
    uint32_t rx = 0;
    bool success = false;

    for (int i = 0; i < 3; i++) {
        esp_ping_get_profile(ping, ESP_PING_PROF_REPLY, &rx, sizeof(rx));
        if (rx > 0) { success = true; break; }
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    esp_ping_stop(ping);
    esp_ping_delete_session(ping);
    return success;
}

/* -------------------------------------------------------
 * Mevcut bağlantıyı durdur
 * ------------------------------------------------------- */
static void stop_current_connection(void)
{
    switch (s_current_mode) {
    case NET_MODE_ETHERNET:
        ESP_LOGW(TAG, "Ethernet durduruluyor...");
        stop_w5500_ethernet();
        s_eth_link_up = false;
        break;

    case NET_MODE_WIFI:
        ESP_LOGW(TAG, "Wi-Fi durduruluyor...");
        stop_wifi_station();
        s_wifi_connected = false;
        break;

    case NET_MODE_GSM:
        ESP_LOGW(TAG, "GSM durduruluyor... (TODO)");
        break;

    default:
        break;
    }

    vTaskDelay(pdMS_TO_TICKS(800));
}

/* -------------------------------------------------------
 * Ağ başlatıcı
 * ------------------------------------------------------- */
static void start_network(void)
{
    ensure_event_loop_initialized();

    switch (s_current_mode) {
    case NET_MODE_ETHERNET:
        ESP_LOGI(TAG, "[ETH] Ethernet başlatılıyor...");
        start_w5500_ethernet();
        break;

    case NET_MODE_WIFI:
        ESP_LOGI(TAG, "[WIFI] Wi-Fi başlatılıyor...");
        start_wifi_station();
        break;

    case NET_MODE_GSM:
        ESP_LOGI(TAG, "[GSM] GSM başlatılıyor... (TODO)");
        break;

    default:
        break;
    }
}

/* -------------------------------------------------------
 * Ana görev (failover + BLE override)
 * ------------------------------------------------------- */
static void net_manager_task(void *arg)
{
    ESP_LOGI(TAG, "Ağ yönetim görevi başlatıldı.");

    // Başlangıçta uygun modu seç
    if (ethernet_available()) s_current_mode = NET_MODE_ETHERNET;
    else s_current_mode = NET_MODE_WIFI;

    start_network();

    while (1)
    {
        /* ---- BLE Manuel geçiş varsa ---- */
        if (s_manual_override) {
            s_manual_override = false;
            stop_current_connection();
            s_current_mode = s_manual_mode;
            ESP_LOGW(TAG, "BLE ile manuel olarak %d moduna geçiliyor.", s_current_mode);
            start_network();
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        bool connected = false;
        bool ping_ok = false;

        switch (s_current_mode) {
            /* ---------------- AUTO ---------------- */
            case NET_MODE_AUTO:
                ESP_LOGI(TAG, "[AUTO] Otomatik mod: Ethernet var mı kontrol ediliyor...");
                if (ethernet_available())
                    s_current_mode = NET_MODE_ETHERNET;
                else
                    s_current_mode = NET_MODE_WIFI;
                start_network();
                break;

            /* ---------------- Ethernet ---------------- */
            case NET_MODE_ETHERNET:
                connected = s_eth_link_up;
                if (!connected) {
                    ESP_LOGW(TAG, "[ETH] Link DOWN → Wi-Fi’ye geçiliyor.");
                    stop_current_connection();
                    s_current_mode = NET_MODE_WIFI;
                    start_network();
                    break;
                }

                ping_ok = ping_test();
                if (ping_ok) {
                    ESP_LOGI(TAG, "[ETH] İnternet aktif, veri gönderilebilir.");
                } else {
                    ESP_LOGW(TAG, "[ETH] Ping başarısız → Wi-Fi’ye geçiliyor.");
                    stop_current_connection();
                    s_current_mode = NET_MODE_WIFI;
                    start_network();
                }
                break;

            /* ---------------- Wi-Fi ---------------- */
            case NET_MODE_WIFI:
                connected = wifi_is_connected();
                if (!connected) {
                    ESP_LOGW(TAG, "[WIFI] Bağlantı kurulamadı → GSM’e geçiliyor.");
                    stop_current_connection();
                    s_current_mode = NET_MODE_GSM;
                    start_network();
                    break;
                }

                ping_ok = ping_test();
                if (ping_ok) {
                    ESP_LOGI(TAG, "[WIFI] İnternet aktif, veri gönderilebilir.");
                } else {
                    ESP_LOGW(TAG, "[WIFI] Ping başarısız → GSM’e geçiliyor.");
                    stop_current_connection();
                    s_current_mode = NET_MODE_GSM;
                    start_network();
                }
                break;

            /* ---------------- GSM ---------------- */
            case NET_MODE_GSM:
                // Burada gsm_internet_ok() gelecekte eklenecek.
                ESP_LOGI(TAG, "[GSM] GSM kontrolü (şimdilik varsayılan başarısız).");
                bool gsm_ok = false;
                if (!gsm_ok) {
                    ESP_LOGW(TAG, "[GSM] GSM başarısız → Ethernet’e geçiliyor.");
                    stop_current_connection();
                    s_current_mode = NET_MODE_ETHERNET;
                    start_network();
                } else {
                    ESP_LOGI(TAG, "[GSM] İnternet aktif, veri gönderilebilir.");
                }
                break;
            }

            vTaskDelay(pdMS_TO_TICKS(3000)); // 3 saniyede bir döngü
        }
}

/* -------------------------------------------------------
 * Görev oluşturucu
 * ------------------------------------------------------- */
void net_manager_create_task(void)
{
    xTaskCreatePinnedToCore(net_manager_task, "net_manager_task", 8192, NULL, 5, NULL, 1);
    ESP_LOGI(TAG, "Ağ yöneticisi görevi oluşturuldu.");
}
