
idf_component_register(SRCS "app_main.c"
                    INCLUDE_DIRS "."
                    REQUIRES App_Tasks Sensor PressureSensor Wifi_Connect Mqtt_Connect LED KEY Timestamp LCD_Driver
                    )
 