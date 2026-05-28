idf_component_register(SRCS "Wifi_Connect.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_event esp_netif esp_wifi joltwallet__littlefs esp_wifi_manager esp_bus log)
