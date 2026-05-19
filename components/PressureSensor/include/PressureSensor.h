#ifndef PRESSURE_SENSOR_H
#define PRESSURE_SENSOR_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "App_Data.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRESSURE_SENSOR_COUNT (2)


typedef enum {
	PRESSURE_SENSOR_WORK_MODE_PHONE = 0x00,
	PRESSURE_SENSOR_WORK_MODE_GATEWAY = 0xFF,
} PressureSensorWorkMode_t;

typedef struct {
	float low_alarm_pa;
	float high_alarm_pa;
	float oscillation_alarm_pa;
} PressureSensorAlarmConfig_t;

typedef struct {
	bool set_work_mode;
	PressureSensorWorkMode_t work_mode;
	bool set_sample_period_ms;
	uint32_t sample_period_ms;
	bool set_report_period_ms;
	uint32_t report_period_ms;
	bool set_alarm;
	PressureSensorAlarmConfig_t alarm;
} PressureSensorConfigRequest_t;

typedef struct {
	PressureSensorData_t pressure;
	uint8_t device_type;
	uint8_t alarm_status;
	uint32_t sample_period_ms;
	uint32_t sample_period_min_ms;
	uint32_t sample_period_max_ms;
	uint32_t report_period_ms;
	uint32_t report_period_min_ms;
	uint32_t report_period_max_ms;
	PressureSensorAlarmConfig_t alarm;
	PressureSensorWorkMode_t work_mode;
	bool connected;
	bool data_valid;
	bool config_valid;
} PressureSensorState_t;

/**
 * @brief 压力传感器数据回调函数类型。
 * @param sensor_index 传感器索引，0/1 对应两路设备。
 * @param data 最新压力数据。
 * @param user_ctx 用户上下文指针。
 */
typedef void (*PressureSensorDataCallback_t)(uint8_t sensor_index,
										 const PressureSensorData_t *data,
										 void *user_ctx);

/**
 * @brief 初始化压力传感器 BLE 管理模块。
 */
esp_err_t PressureSensor_Init(void);

/**
 * @brief 注册压力数据回调，用于 notify 后直接推送上层。
 */
esp_err_t PressureSensor_RegisterDataCallback(PressureSensorDataCallback_t callback, void *user_ctx);

/**
 * @brief 获取指定压力传感器当前状态。
 */
esp_err_t PressureSensor_GetState(uint8_t sensor_index, PressureSensorState_t *out_state);

/**
 * @brief 读取指定压力传感器的最新压力值。
 */
esp_err_t pressure_sensor_read(uint8_t sensor_index, PressureSensorData_t *out_data);

/**
 * @brief 获取所有压力传感器状态。
 */
esp_err_t PressureSensor_GetAllState(PressureSensorState_t out_state[PRESSURE_SENSOR_COUNT]);

/**
 * @brief 设置压力传感器工作模式。
 */
esp_err_t PressureSensor_SetWorkMode(uint8_t sensor_index, PressureSensorWorkMode_t mode);

/**
 * @brief 设置压力采集周期。
 */
esp_err_t PressureSensor_SetSamplePeriodMs(uint8_t sensor_index, uint32_t period_ms);

/**
 * @brief 设置压力上报周期。
 */
esp_err_t PressureSensor_SetReportPeriodMs(uint8_t sensor_index, uint32_t period_ms);

/**
 * @brief 设置压力报警参数。
 */
esp_err_t PressureSensor_SetAlarmSettings(uint8_t sensor_index, const PressureSensorAlarmConfig_t *alarm);

/**
 * @brief 按请求结构批量应用压力传感器配置。
 */
esp_err_t PressureSensor_ApplyConfig(uint8_t sensor_index, const PressureSensorConfigRequest_t *config);

#ifdef __cplusplus
}
#endif

#endif
