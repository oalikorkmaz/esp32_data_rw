idf_component_register(
    SRCS "storage_spiffs.c"
    INCLUDE_DIRS "include"
    REQUIRES fatfs sdmmc driver spi_if cfg_if time_if
)
spiffs_create_partition_image(storage spiffs_image FLASH_IN_PROJECT)
