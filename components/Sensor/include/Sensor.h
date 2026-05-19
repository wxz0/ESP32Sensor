#ifndef WATERQUALITYSENSOR_H
#define WATERQUALITYSENSOR_H

#include "esp_err.h"
#include "App_Data.h"

esp_err_t WaterQualitySensor_init(void);

esp_err_t water_sensor_read_all(WaterQualityData_t *out_data);

esp_err_t light_sensor_read_all(LightSensorData_t *out_data);

esp_err_t water_sensor_calib_ph_4_01(void);

esp_err_t water_sensor_calib_ph_6_86(void);

esp_err_t water_sensor_calib_ph_9_18(void);

esp_err_t read_light_address(uint8_t light_adr[]);
#endif
