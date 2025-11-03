# main bileşeninin kaynak dosyalarını tanımla
idf_component_register(SRCS "app_main.c" "state_machine.c"
                       REQUIRES cfg_if time_if storage_if net_if parser ble_system driver
                        INCLUDE_DIRS ".")