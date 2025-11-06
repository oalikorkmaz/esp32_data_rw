#include "data_parser.h"
#include "esp_log.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>

static const char *TAG = "DATA_PARSER";

static sensor_info_t sensor_map[MAX_SENSORS];
static int sensor_map_count = 0;

/* -----------------------------------------
 * Sensör Haritası Yönetimi
 * ----------------------------------------- */
void parser_reset_sensor_map(void)
{
    sensor_map_count = 0;
}

void parser_add_sensor(const char *name, const char *unit)
{
    if (sensor_map_count >= MAX_SENSORS) return;
    strncpy(sensor_map[sensor_map_count].name, name, sizeof(sensor_map[0].name) - 1);
    strncpy(sensor_map[sensor_map_count].unit, unit, sizeof(sensor_map[0].unit) - 1);
    sensor_map_count++;
}

/* -----------------------------------------
 * Delta Ohm RS232 Satır Çözümleyici
 * ----------------------------------------- */
bool parse_hd32mt_record(const char *raw_line, hd32mt_data_t *out)
{
    if (!raw_line || !out) return false;

    // 1️⃣ Satır tipi kontrolü
    if (strncmp(raw_line, "$R0", 3) != 0 && strncmp(raw_line, "$A0", 3) != 0)
        return false;

    // 2️⃣ Tarih etiketini bul
    const char *ts = strstr(raw_line, "24");  // "240801120831" gibi
    if (!ts) return false;
    strncpy(out->timestamp, ts, 12);
    out->timestamp[12] = '\0';

    // 3️⃣ Tarihi biçimlendir
    snprintf(out->timestamp_full, sizeof(out->timestamp_full),
             "20%c%c-%c%c-%c%c %c%c:%c%c:%c%c",
             ts[0], ts[1], ts[2], ts[3],
             ts[4], ts[5], ts[6], ts[7],
             ts[8], ts[9], ts[10], ts[11]);

    // 4️⃣ Binary veri kısmını bul
    const char *data_start = strchr(ts, ' ');
    if (!data_start) return false;
    data_start++;
    const char *end = strchr(data_start, '&');
    if (!end) return false;

    size_t data_len = end - data_start;
    if (data_len < 4) return false;

    // 5️⃣ Float çözümleme (big endian)
    out->sensor_count = 0;
    for (int i = 0; i + 4 <= data_len && out->sensor_count < MAX_SENSORS; i += 4) {
        uint8_t b[4];
        b[0] = (uint8_t)data_start[i + 3];
        b[1] = (uint8_t)data_start[i + 2];
        b[2] = (uint8_t)data_start[i + 1];
        b[3] = (uint8_t)data_start[i + 0];
        float value;
        memcpy(&value, b, 4);

        if (value == value && value > -1e6f && value < 1e6f) {
            int idx = out->sensor_count;
            out->sensors[idx] = value;
            out->labels[idx] = (idx < sensor_map_count) ? sensor_map[idx].name : "Unknown";
            out->units[idx]  = (idx < sensor_map_count) ? sensor_map[idx].unit : "";
            out->sensor_count++;
        }
    }

    ESP_LOGI(TAG, "Tarih: %s (%d sensör)", out->timestamp_full, out->sensor_count);
    for (int k = 0; k < out->sensor_count; k++) {
        ESP_LOGI(TAG, "  [%s] = %.2f %s", out->labels[k], out->sensors[k], out->units[k]);
    }

    return (out->sensor_count > 0);
}
