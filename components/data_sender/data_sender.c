#include "data_sender.h"
#include "cfg_if.h"
#include "net_manager.h"

#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include <string.h>
#include <stdio.h>
#include "data_parser.h"

static const char *LOG_TAG_DATA_SENDER = "DATA_SENDER";

/* Gönderilecek satır için makul bir üst sınır (device + ts + 32 kanal) */
#define DATA_SENDER_MAX_LINE_BYTES  512

/* ---- Device ID geçici override (sadece test için) ---- */
static char device_id_override_buf[64] = {0};



void data_sender_set_device_id_override(const char *device_id_override)
{
    if (device_id_override && device_id_override[0] != '\0') {
        snprintf(device_id_override_buf, sizeof device_id_override_buf, "%s", device_id_override);
    } else {
        device_id_override_buf[0] = '\0'; // override kapat
    }
}
/* ---------------------- TCP yardımcıları ---------------------- */

/**
 * Tek satırı (NULL sonlandırıcı hariç) RAW TCP ile gönderir.
 * Hercules davranışına benzer: TCP_NODELAY açık, write sonrası SHUT_WR,
 * kısa bir recv ile "$SEND$" beklenir ve alınır alınmaz soket kapatılır.
 */
static bool tcp_send_single_line_and_close(const char *server_host,
                                           int         server_port,
                                           const char *line_to_send)
{
    if (!server_host || !line_to_send || server_port <= 0) {
        ESP_LOGE(LOG_TAG_DATA_SENDER, "tcp_send_single_line_and_close: invalid args");
        return false;
    }

    struct addrinfo hints = {0}, *addr_list = NULL;
    char server_port_str[16];
    int n = snprintf(server_port_str, sizeof server_port_str, "%d", server_port);
    if (n < 0 || n >= (int)sizeof(server_port_str)) {
        ESP_LOGE(LOG_TAG_DATA_SENDER, "Port string overflow");
        return false;
}

    hints.ai_socktype = SOCK_STREAM;

    int gai = getaddrinfo(server_host, server_port_str, &hints, &addr_list);
    if (gai != 0 || !addr_list) {
        ESP_LOGE(LOG_TAG_DATA_SENDER, "getaddrinfo failed: %d", gai);
        return false;
    }

    int socket_fd = -1;
    for (struct addrinfo *it = addr_list; it; it = it->ai_next) {
        socket_fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (socket_fd < 0) continue;

        int one = 1;
        setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);

        struct timeval io_timeout = { .tv_sec = 5, .tv_usec = 0 };
        setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &io_timeout, sizeof io_timeout);
        setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &io_timeout, sizeof io_timeout);

        if (connect(socket_fd, it->ai_addr, it->ai_addrlen) == 0) break;

        close(socket_fd);
        socket_fd = -1;
    }
    freeaddrinfo(addr_list);

    if (socket_fd < 0) {
        ESP_LOGE(LOG_TAG_DATA_SENDER, "connect failed");
        return false;
    }

    size_t line_length = strlen(line_to_send);
    ssize_t send_result = send(socket_fd, line_to_send, line_length, 0);
    if (send_result != (ssize_t)line_length) {
        ESP_LOGE(LOG_TAG_DATA_SENDER, "send failed (%d/%u)", (int)send_result, (unsigned)line_length);
        close(socket_fd);
        return false;
    }

    // Yazmayı kapat: sunucuya “gönderim bitti” sinyali
    shutdown(socket_fd, SHUT_WR);

    // Kısa bir yanıt bekle (çoğu sunucu "$SEND$" döner). Görür görmez kapat.
    char response_buffer[64];
    int recv_len = recv(socket_fd, response_buffer, sizeof(response_buffer) - 1, 0);
    if (recv_len > 0) {
        response_buffer[recv_len] = '\0';
        ESP_LOGI(LOG_TAG_DATA_SENDER, "TCP RESPONSE: %s", response_buffer);
        // "$SEND$" yakalandığında daha fazla beklemiyoruz.
    }

    close(socket_fd);
    return true;
}

/* -------------------- Frame oluşturma ve gönderim -------------------- */

bool data_sender_send_frame_from_record(const hd32mt_data_t  *record,
                                        int total_channels,
                                        const char *formatted_timestamp)
{
    if (!record || total_channels <= 0 || !formatted_timestamp || formatted_timestamp[0] == '\0') {
        ESP_LOGE(LOG_TAG_DATA_SENDER, "data_sender_send_frame_from_record: invalid args");
        return false;
    }

    const device_cfg_t *device_config = cfg_get();
    if (!device_config) {
        ESP_LOGE(LOG_TAG_DATA_SENDER, "device_config is null");
        return false;
    }

    if (!net_manager_is_connected()) {
        ESP_LOGW(LOG_TAG_DATA_SENDER, "network not connected, frame not sent");
        return false;
    }

    const char *effective_device_id =
        (device_id_override_buf[0] != '\0') ? device_id_override_buf
                                            : device_config->device_id;

    // $<device>$<yy/mm/dd-HH:MM:SS>$<N>$
    char frame_line[DATA_SENDER_MAX_LINE_BYTES];
    size_t write_offset = 0;

    int written = snprintf(frame_line + write_offset,
                           sizeof(frame_line) - write_offset,
                           "$%s$%s$%d$",
                           effective_device_id,
                           formatted_timestamp,
                           total_channels);
    if (written < 0 || (size_t)written >= sizeof(frame_line) - write_offset) {
        ESP_LOGE(LOG_TAG_DATA_SENDER, "frame header overflow");
        return false;
    }
    write_offset += (size_t)written;

    // Kanal değerlerini sırayla yaz (eksikse 0.00 ile doldur, fazlaysa kes)
    for (int channel_index = 1; channel_index <= total_channels; ++channel_index) {
        float channel_value = 0.0f;
        int   record_index  = channel_index - 1;

        if (record_index < record->sensor_count) {
            channel_value = record->sensors[record_index];
        }

        written = snprintf(frame_line + write_offset,
                           sizeof(frame_line) - write_offset,
                           "%.2f$",
                           (double)channel_value);
        if (written < 0 || (size_t)written >= sizeof(frame_line) - write_offset) {
            ESP_LOGE(LOG_TAG_DATA_SENDER, "frame body overflow at ch=%d", channel_index);
            return false;
        }
        write_offset += (size_t)written;
    }

    // Satırı CRLF ile bitir
    written = snprintf(frame_line + write_offset,
                       sizeof(frame_line) - write_offset,
                       "\r\n");
    if (written < 0 || (size_t)written >= sizeof(frame_line) - write_offset) {
        ESP_LOGE(LOG_TAG_DATA_SENDER, "frame ending overflow");
        return false;
    }

    ESP_LOGI(LOG_TAG_DATA_SENDER, "FRAME TO SEND: %s", frame_line);

    // TCP yoluyla gönder
    return tcp_send_single_line_and_close(device_config->server_host,
                                          (int)device_config->server_port,
                                          frame_line);
}
