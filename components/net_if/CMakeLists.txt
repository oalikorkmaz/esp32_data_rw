idf_component_register(SRCS 
                            "net_eth_service.c"
                            
                            "ethernet_init.c"
                            "net_manager.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_event esp_netif esp_eth driver)