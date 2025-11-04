#ifndef WIFI_INIT_H
#define WIFI_INIT_H

#include "esp_err.h"
#include <stdbool.h>

esp_err_t start_wifi_station(void);
bool wifi_is_connected(void);

#endif
