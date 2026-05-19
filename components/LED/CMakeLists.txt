idf_component_register(SRCS "LED.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_driver_gpio log Common)
