#ifndef USB_STORAGE_H
#define USB_STORAGE_H

#include "App_Data.h"

void Usb_Storage_Init(void);

void Usb_Storage_LogWater(const WaterQualityData_t *data);
void Usb_Storage_LogLight(const LightSensorData_t *data);
void Usb_Storage_LogPressure(uint8_t sensor_index, const PressureSensorData_t *data);

#endif
