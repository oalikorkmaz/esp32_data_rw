idf_component_register(SRCS "net_if.c"
                            "net_wifi.c"
                            "net_eth_service.c"
                            "net_gsm_stub.c"
                            "ethernet_init.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_event esp_netif esp_eth driver)