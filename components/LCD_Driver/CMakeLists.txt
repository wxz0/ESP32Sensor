idf_component_register(
    SRCS "LCD_Driver.c"
    INCLUDE_DIRS "include"
    REQUIRES
        Common
        UI
        driver
        esp_lcd
        esp_timer
        freertos
        lvgl__lvgl
)
