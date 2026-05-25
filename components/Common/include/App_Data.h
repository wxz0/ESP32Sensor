#ifndef APP_DATA_H
#define APP_DATA_H
#include "stdint.h"
#include "time.h"

#define Device_ID "device001"

/*水质量数据结构体*/
typedef struct 
{  
  float temp;   
  float ph;
  uint32_t tds;
  int16_t orp;
  float salt;
  float saltppt;
  float sg;
  uint32_t ec;
  float cl;
  time_t timestamp;      
} WaterQualityData_t;

/*光强传感器数据结构体*/
typedef struct 
{
  float lux_1;
  float lux_2;
  float lux_3;
  float lux_4;
  time_t timestamp;
}LightSensorData_t;

/*压力传感器数据结构体*/
typedef struct
{
  float pressure_cur;//当前压力值
  float pressure_min;//最低压力值
  float pressure_max;//最高压力值
  time_t timestamp;
}PressureSensorData_t;

/*设备配置结构体*/
typedef struct 
{
  char device_id[32];
}DeviceConfig_t;

/*LED枚举*/
typedef enum
{
 LED1 = 0,
 LED2
}LED_x;

/*按键枚举*/
typedef enum
{
  KEY1 = 1,
  KEY2,
  KEY3,
  KEY4
}KEY_x;

/*传感器消息类型枚举*/
typedef enum {
  SENSOR_MSG_WATER = 1,
  SENSOR_MSG_LIGHT,
  SENSOR_MSG_PRESSURE1,
  SENSOR_MSG_PRESSURE2
} SensorMsgType_t;

/** 传感器消息结构体 */
typedef struct {
  SensorMsgType_t type;
  union {
    WaterQualityData_t water;
    LightSensorData_t light;
    PressureSensorData_t pressure;
  } data;
} SensorMessage_t;


#endif