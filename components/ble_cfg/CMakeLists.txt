idf_component_register(SRCS "ble_cfg_service.c"
                       INCLUDE_DIRS "include"
                       REQUIRES bt driver esp_driver_gpio nvs_flash)
