idf_component_register(SRCS "camera.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_camera esp_http_server nvs_flash esp_wifi esp_event freertos driver)
