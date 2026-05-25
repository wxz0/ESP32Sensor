idf_component_register(SRCS "App_Tasks.c"
                    INCLUDE_DIRS "include"
                    REQUIRES Sensor PressureSensor Mqtt_Connect Json_Utils KEY Timestamp Common freertos log LCD_Driver)
