idf_component_register(
    SRCS "telemetry_service.c" "serial_if.c"
    INCLUDE_DIRS "include"
    REQUIRES storage_if net_if time_if data_sender data_parser esp_event
)
