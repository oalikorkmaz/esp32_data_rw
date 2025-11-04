idf_component_register(SRCS 
                            "net_eth_service.c"
                            "wifi_init.c"
                            "ethernet_init.c"
                            "net_manager.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_event esp_netif esp_eth driver esp_wifi nvs_flash)