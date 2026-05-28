#ifndef LCD_DRIVER_H
#define LCD_DRIVER_H

#include <stdbool.h>

#include "esp_err.h"
#include "App_Data.h"

esp_err_t LCD_Driver_Init(void);
bool LCD_Driver_IsReady(void);
void lcd_switch_to_main(void);
void lcd_switch_to_light(void);
void lcd_show_calibration(const char *name);
void lcd_update_calibration_status(const char *name,
                                   int remaining_sec,
                                   int total_sec,
                                   const char *status);

void lcd_ui_update_sensor_data(const WaterQualityData_t *water,
                               const LightSensorData_t *light,
                               const PressureSensorData_t *press1,
                               const PressureSensorData_t *press2);

#endif
