#include "net_if.h"
#include "esp_log.h"

static const char *TAG = "NET_WIFI";

esp_err_t net_init(void) {
    ESP_LOGI(TAG, "NET_IF bileşeni başlatılıyor (WiFi).");
    // Burada ESP-NETIF ve temel event handler başlatma kodu olacak.
    
    return ESP_OK;
}
// Diğer .c dosyaları (net_eth_stub.c, net_gsm_stub.c) için de birer init fonksiyonu oluşturmayı unutmayın.