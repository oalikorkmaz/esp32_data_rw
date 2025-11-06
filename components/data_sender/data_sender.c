#include "data_sender.h"
#include "cfg_if.h"
#include "net_manager.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "DATA_SENDER";

/**
 * @brief Tek bir sensör değerini sunucuya gönderir.
 * 
 * Format örneği:
 * $$ESP32-A1B2C3D4$$2024-08-01 12:08:31$$Albedo$$55.5$$W/m2$$\r\n
 */
bool data_sender_send(const char *label, float value, const char *unit, const char *timestamp)
{
    const device_cfg_t *cfg = cfg_get();
    if (!cfg) {
        ESP_LOGE(TAG, "cfg_get() başarısız!");
        return false;
    }

    // 🌐 İnternet var mı kontrol et
    if (!net_manager_is_connected()) {
        ESP_LOGW(TAG, "Ağ bağlantısı yok, veri gönderilmeyecek.");
        return false;
    }

    // --- 1. Gönderilecek veri hazırlanıyor ---
    char payload[256];
    snprintf(payload, sizeof(payload),
             "$$%s$$%s$$%s$$%.2f$$%s$$\r\n",
             cfg->device_id,                        // cihaz kimliği
             timestamp ? timestamp : "-",           // tarih/saat
             label ? label : "UNKNOWN",             // sensör etiketi
             value,                                 // sensör değeri
             unit ? unit : "-");                    // birim (örn. W/m2)

    ESP_LOGI(TAG, "Gönderiliyor: %s", payload);

    // --- 2. Sunucu adresini çöz ---
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(cfg->server_port);

    struct hostent *he = gethostbyname(cfg->server_host);
    if (!he) {
        ESP_LOGE(TAG, "Sunucu DNS çözümleme hatası: %s", cfg->server_host);
        return false;
    }
    memcpy(&dest_addr.sin_addr, he->h_addr, he->h_length);

    // --- 3. TCP soketi oluştur ---
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Soket oluşturulamadı: errno=%d", errno);
        return false;
    }

    // --- 4. Sunucuya bağlan ---
    if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
        ESP_LOGE(TAG, "Sunucuya bağlanılamadı: %s:%d (errno=%d)",
                 cfg->server_host, cfg->server_port, errno);
        close(sock);
        return false;
    }

    // --- 5. Veriyi gönder ---
    int sent = send(sock, payload, strlen(payload), 0);
    if (sent < 0) {
        ESP_LOGE(TAG, "Veri gönderilemedi: errno=%d", errno);
        close(sock);
        return false;
    }

    // --- 6. Cevap bekle (isteğe bağlı) ---
    char rx_buffer[128];
    int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
    if (len > 0) {
        rx_buffer[len] = '\0';
        ESP_LOGI(TAG, "Sunucudan cevap: %s", rx_buffer);
    } else {
        ESP_LOGW(TAG, "Sunucudan cevap alınmadı.");
    }

    close(sock);
    ESP_LOGI(TAG, "Veri gönderimi tamamlandı.");
    return true;
}
