#ifndef NET_ETH_IF_H
#define NET_ETH_IF_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t net_eth_start(void);
esp_err_t net_eth_stop(void);


#ifdef __cplusplus
}
#endif

#endif // NET_ETH_IF_H
