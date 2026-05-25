#ifndef LCD_DRIVER_H
#define LCD_DRIVER_H

#include "App_Data.h"

/**
 * @brief Initialize LCD hardware + LVGL + UI screens
 */
void LCD_Driver_Init(void);

/**
 * @brief Switch to main data display screen (water quality + pressure)
 */
void lcd_switch_to_main(void);

/**
 * @brief Switch to light sensor display screen
 */
void lcd_switch_to_light(void);

/**
 * @brief Update sensor data on current screen
 * @param water   Water quality data (NULL if not available)
 * @param light   Light sensor data (NULL if not available)
 * @param press1  Pressure sensor 1 data (NULL if not available)
 * @param press2  Pressure sensor 2 data (NULL if not available)
 */
void lcd_ui_update_sensor_data(const WaterQualityData_t *water,
                               const LightSensorData_t *light,
                               const PressureSensorData_t *press1,
                               const PressureSensorData_t *press2);

/**
 * @brief Show calibration waiting screen
 * @param name Calibration name string (e.g. "EC", "PH", "ORP")
 */
void lcd_show_calibration(const char *name);

#endif
