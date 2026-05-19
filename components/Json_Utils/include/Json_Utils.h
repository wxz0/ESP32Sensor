#ifndef JSON_UTILS_H
#define JSON_UTILS_H
#include "cJSON.h"
#include "App_Data.h"

char * JSON_PackSensorData(WaterQualityData_t* data);
char * JSON_PackLightData(LightSensorData_t* data);
char * JSON_PackPressureData(PressureSensorData_t* data);

#endif