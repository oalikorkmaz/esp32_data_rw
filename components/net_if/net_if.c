#include "net_if.h"
#include "esp_log.h"
#include "net_eth_if.h"
// #include "net_wifi_if.h"
// #include "net_gsm_if.h"

static const char *TAG = "NET_IF";

static net_mode_t s_current_mode = NET_MODE_NONE;

// ----------------------------------------------------
// Ağ yığını genel başlatma
// ----------------------------------------------------
esp_err_t net_init(void)
{
    ESP_LOGI(TAG, "Ağ yığını başlatılıyor...");

    // Ortak altyapı burada başlatılır (örneğin NVS, netif, event_loop)
    // Şu anda ethernet modülümüz kendi içinde netif_init yapıyor, o yüzden burası sade.
    s_current_mode = NET_MODE_NONE;
    return ESP_OK;
}

// ----------------------------------------------------
// Ağ modu değiştirme
// ----------------------------------------------------
esp_err_t net_set_mode(net_mode_t mode)
{
    if (mode == s_current_mode) {
        ESP_LOGW(TAG, "Zaten istenen mod aktif: %d", mode);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Yeni ağ modu seçildi: %d", mode);

    // Önce mevcut bağlantıyı kapat
    switch (s_current_mode) {
        case NET_MODE_ETHERNET:
            net_eth_stop();
            break;
        // case NET_MODE_WIFI:
        //     net_wifi_stop();
        //     break;
        // case NET_MODE_GSM:
        //     net_gsm_stop();
        //     break;
        default:
            break;
    }

    // Yeni modu başlat
    switch (mode) {
        case NET_MODE_ETHERNET:
            ESP_ERROR_CHECK(net_eth_init());
            ESP_ERROR_CHECK(net_eth_start());
            break;

        // case NET_MODE_WIFI:
        //     ESP_ERROR_CHECK(net_wifi_init());
        //     ESP_ERROR_CHECK(net_wifi_start());
        //     break;

        // case NET_MODE_GSM:
        //     ESP_ERROR_CHECK(net_gsm_init());
        //     ESP_ERROR_CHECK(net_gsm_start());
        //     break;

        default:
            ESP_LOGW(TAG, "Bilinmeyen mod veya NET_MODE_NONE seçildi.");
            break;
    }

    s_current_mode = mode;
    return ESP_OK;
}

// ----------------------------------------------------
// Aktif mod ve bağlantı durumu
// ----------------------------------------------------
net_mode_t net_get_current_mode(void)
{
    return s_current_mode;
}

bool net_is_connected(void)
{
    switch (s_current_mode) {
        case NET_MODE_ETHERNET:
            return net_eth_is_connected();
        // case NET_MODE_WIFI:
        //     return net_wifi_is_connected();
        // case NET_MODE_GSM:
        //     return net_gsm_is_connected();
        default:
            return false;
    }
}

// ----------------------------------------------------
// Veri gönderme (şimdilik boş bırakıyoruz)
// ----------------------------------------------------
esp_err_t net_send_data(const uint8_t *data_buffer, size_t data_len)
{
    if (!net_is_connected()) {
        ESP_LOGW(TAG, "Ağ bağlı değil, veri gönderilemez.");
        return ESP_FAIL;
    }

    switch (s_current_mode) {
        case NET_MODE_ETHERNET:
            // Burada ileride HTTP veya MQTT üzerinden gönderim yapılacak
            ESP_LOGI(TAG, "[ETH] %u bayt veri gönderilecek (henüz uygulanmadı)", data_len);
            break;

        // case NET_MODE_WIFI:
        //     ...
        // case NET_MODE_GSM:
        //     ...
        default:
            break;
    }

    return ESP_OK;
}
