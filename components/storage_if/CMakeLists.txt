idf_component_register(
    SRCS "storage_spiffs.c"
    INCLUDE_DIRS "include"
    REQUIRES fatfs sdmmc driver spi_if
)