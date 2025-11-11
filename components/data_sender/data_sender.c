#include "data_sender.h"
#include "cfg_if.h"
#include "net_manager.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "data_parser.h"
#include "time_if.h"
#include "storage_spiffs.h"

#include <string.h>
#include <stdio.h>

#define DATA_SENDER_MAX_LINE_BYTES 512
static const char *TAG = "DATA_SENDER";

/* ==========================================================
 * 1️⃣ FRAME OLUŞTURMA
 * ========================================================== */
static bool data_sender_build_frame(const hd32mt_data_t *record,
                                    int total_channels,
                                    const char *manual_timestamp,
                                    char *out_frame,
                                    size_t out_cap)
{
    if (!record || !out_frame || out_cap == 0) return false;

    char timestamp[24];
    if (manual_timestamp && strlen(manual_timestamp) > 5) {
        strncpy(timestamp, manual_timestamp, sizeof(timestamp));
        timestamp[sizeof(timestamp) - 1] = '\0';
    } else {
        time_if_get_formatted_timestamp(timestamp, sizeof(timestamp)); 
    }

    const device_cfg_t *cfg = cfg_get();
    if (!cfg) {
        ESP_LOGE(TAG, "Config not available!");
        return false;
    }
    const char *manual_device_id = "00-08-DC-20-00-59";
    size_t offset = 0;
    int written = snprintf(out_frame + offset, out_cap - offset,
                           "$%s$%s$%d$",
                           manual_device_id, //cfg->device_id,
                           timestamp,
                           total_channels);
    if (written < 0 || (size_t)written >= out_cap - offset)
        return false;
    offset += written;

    for (int i = 0; i < total_channels; ++i) {
        float val = (i < record->sensor_count) ? record->sensors[i] : 0.0f;
        written = snprintf(out_frame + offset, out_cap - offset, "%.2f$", val);
        if (written < 0 || (size_t)written >= out_cap - offset)
            return false;
        offset += written;
    }

    snprintf(out_frame + offset, out_cap - offset, "\r\n");
    return true;
}

/* ==========================================================
 * 2️⃣ SUNUCUYA GÖNDERME
 * ========================================================== */
static bool data_sender_send_to_server(const char *frame)
{
    const device_cfg_t *cfg = cfg_get();
    if (!cfg || !frame) return false;

    if (!net_manager_is_connected()) {
        ESP_LOGW(TAG, "Network not connected");
        return false;
    }

    struct addrinfo hints = {0}, *res = NULL;
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%ld", cfg->server_port);
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(cfg->server_host, port_str, &hints, &res) != 0 || !res) {
        ESP_LOGE(TAG, "getaddrinfo failed");
        return false;
    }

    int sock = -1;
    for (struct addrinfo *it = res; it; it = it->ai_next) {
        sock = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (sock < 0) continue;

        int one = 1;
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

        struct timeval timeout = { .tv_sec = 5, .tv_usec = 0 };
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        if (connect(sock, it->ai_addr, it->ai_addrlen) == 0)
            break;

        close(sock);
        sock = -1;
    }
    freeaddrinfo(res);

    if (sock < 0) {
        ESP_LOGE(TAG, "connect failed");
        return false;
    }

    size_t len = strlen(frame);
    ssize_t sent = send(sock, frame, len, 0);
    if (sent != (ssize_t)len) {
        ESP_LOGE(TAG, "send failed (%d/%u)", (int)sent, (unsigned)len);
        close(sock);
        return false;
    }

    shutdown(sock, SHUT_WR);  // gönderim bitti sinyali

    // 🔸 kısa bir yanıt bekle
    char resp[64];
    int rcv = recv(sock, resp, sizeof(resp) - 1, 0);
    if (rcv > 0) {
        resp[rcv] = '\0';
        ESP_LOGI(TAG, "Server response: %s", resp);
    }

    close(sock);
    ESP_LOGI(TAG, "Frame sent OK");
    return true;
}


/* ==========================================================
 * 3️⃣ SD KARTA KAYDETME
 * ========================================================== */
static bool data_sender_save_to_sd(const char *frame)
{
    esp_err_t res = storage_write_frame(frame);
    return (res == ESP_OK);
}

/* ==========================================================
 * 4️⃣ KOORDİNE EDİCİ (ANA FONKSİYON)
 * ========================================================== */
bool data_sender_send_frame_from_record(const hd32mt_data_t *record,
                                        int total_channels,
                                        const char *formatted_timestamp)
{

    char frame[DATA_SENDER_MAX_LINE_BYTES];
    if (!data_sender_build_frame(record, total_channels, formatted_timestamp,
                             frame, sizeof(frame))) {
        ESP_LOGE(TAG, "Frame build failed");
        return false;
    }

    bool net_ok = data_sender_send_to_server(frame);
    data_sender_save_to_sd(frame);  // İnternet olsa da olmasa da SD’ye yaz
    return net_ok;
}
