#pragma once
#include <stdbool.h>

#define MAX_SENSORS 10



typedef struct {
    char name[32];
    char unit[8];
} sensor_info_t;

typedef struct {
    char timestamp[13];              // YYMMDDhhmmss
    float sensors[MAX_SENSORS];      // Sensör değerleri
    const char *labels[MAX_SENSORS]; // Sensör isimleri
    const char *units[MAX_SENSORS];  // Sensör birimleri
    int sensor_count;
    char timestamp_full[20];  // "2024-08-01 12:08:31"
} hd32mt_data_t;

/**
 * @brief HD32MT sensör konfigürasyonlarını (isim + birim) belleğe yükler.
 */
void parser_reset_sensor_map(void);
void parser_add_sensor(const char *name, const char *unit);

/**
 * @brief Satır HD32MT formatında ise çözümler.
 * @param raw_line RS232’den gelen satır ("$R0 ...")
 * @param out Çözülmüş veri
 */
bool parse_hd32mt_record(const char *raw_line, hd32mt_data_t *out);
