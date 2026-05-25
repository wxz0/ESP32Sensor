idf_component_register(
    SRCS "LCD_Driver.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_lcd lvgl__lvgl driver UI Common freertos
)