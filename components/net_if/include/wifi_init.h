#ifndef WIFI_INIT_H
#define WIFI_INIT_H

#include "esp_err.h"
#include "esp_netif.h"
#include <stdbool.h>

esp_err_t start_wifi_station(void);
void stop_wifi_station(void);
bool wifi_is_connected(void);
extern esp_netif_t *wifi_netif_handle;


#endif
