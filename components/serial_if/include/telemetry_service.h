#pragma once
#include <stdbool.h>

/**
 * Telemetri hattını başlatır:
 *  - Serial RX görevini başlatır (ham satır üretir)
 *  - Satırları alan bir "işleme" görevini başlatır (parse + send [+yakında SD])
 *
 * @param total_channel_count  Web tarafında beklenen toplam kanal sayısı (ör. 10)
 * @return true                Başarıyla başlatıldıysa
 */
bool telemetry_service_start(int total_channel_count);
bool telemetry_send_test_frame(int total_channels,
                               const char *formatted_timestamp,
                               const float *channel_values,
                               int value_count);
