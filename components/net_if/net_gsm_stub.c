#include "net_if.h"
#include "esp_log.h"

static const char *TAG = "GSM_STUB";

esp_err_t net_init(void) {
    ESP_LOGI(TAG, "GSM_STUB bileşeni başlatılıyor (GSM).");
    // Burada ESP-NETIF ve temel event handler başlatma kodu olacak.
    
    return ESP_OK;
}
// Diğer .c dosyaları (net_eth_stub.c, net_gsm_stub.c) için de birer init fonksiyonu oluşturmayı unutmayın.