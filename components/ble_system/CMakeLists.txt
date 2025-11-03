idf_component_register(
    SRCS 
        "ble_system_manager.c"
        "ble_ctrl_start_stop.c"
        "ble_cfg_service.c"
        "ble_button_control.c"
        "ble_led_indicator.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos nvs_flash bt driver esp_driver_gpio led_strip
)
                       