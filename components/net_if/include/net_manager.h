#ifndef NET_MANAGER_H
#define NET_MANAGER_H

#include "esp_err.h"
#include "esp_eth.h"
#include "esp_netif.h"

typedef enum {
    NET_MODE_AUTO = 0,      // Otomatik seçim (önce Ethernet, sonra Wi-Fi, sonra GSM)
    NET_MODE_ETHERNET,      // 1
    NET_MODE_WIFI,          // 2
    NET_MODE_GSM            // 3
} net_mode_t;

void net_manager_set_mode(net_mode_t mode);
void net_manager_set_eth_handle(esp_eth_handle_t handle,
                                esp_eth_netif_glue_handle_t glue,
                                esp_netif_t *netif);
void net_manager_create_task(void);
void net_manager_on_eth_event(bool link_up);
void net_manager_on_wifi_event(bool connected);
bool net_manager_is_internet_ok(void);




#endif // NET_MANAGER_H
