idf_component_register(SRCS "server.c" "main.c" "tuning.c" "chord.c"
                    PRIV_REQUIRES spi_flash
                    REQUIRES driver esp_wifi nvs_flash esp_netif esp_http_server spiffs
                    INCLUDE_DIRS "")

spiffs_create_partition_image(spiffs ../spiffs FLASH_IN_PROJECT)
