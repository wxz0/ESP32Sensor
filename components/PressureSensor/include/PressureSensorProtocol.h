#ifndef PRESSURE_SENSOR_PROTOCOL_H
#define PRESSURE_SENSOR_PROTOCOL_H

#include <stdint.h>

#include "esp_bt_defs.h"
#include "esp_err.h"

#include "PressureSensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 获取压力传感器服务 UUID。 */
const esp_bt_uuid_t *PressureSensorProto_GetServiceUuid(void);
/** 获取压力数据特征 UUID。 */
const esp_bt_uuid_t *PressureSensorProto_GetPressureCharUuid(void);
/** 获取采集周期特征 UUID。 */
const esp_bt_uuid_t *PressureSensorProto_GetSamplePeriodCharUuid(void);
/** 获取上报周期特征 UUID。 */
const esp_bt_uuid_t *PressureSensorProto_GetReportPeriodCharUuid(void);
/** 获取报警参数特征 UUID。 */
const esp_bt_uuid_t *PressureSensorProto_GetAlarmSettingsCharUuid(void);
/** 获取工作模式特征 UUID。 */
const esp_bt_uuid_t *PressureSensorProto_GetWorkModeCharUuid(void);

/** 解析压力数据 payload。 */
esp_err_t PressureSensorProto_ParsePressureData(const uint8_t *value,
											 uint16_t value_len,
											 PressureSensorState_t *state);

/** 解析周期元数据 payload。 */
esp_err_t PressureSensorProto_ParsePeriodMeta(const uint8_t *value,
											 uint16_t value_len,
											 uint32_t *current_ms,
											 uint32_t *min_ms,
											 uint32_t *max_ms);

/** 解析报警参数 payload。 */
esp_err_t PressureSensorProto_ParseAlarmSettings(const uint8_t *value,
											 uint16_t value_len,
											 PressureSensorAlarmConfig_t *alarm);

/** 解析工作模式 payload。 */
esp_err_t PressureSensorProto_ParseWorkMode(const uint8_t *value,
										 uint16_t value_len,
										 PressureSensorWorkMode_t *mode);

/** 将 Pa 转为协议原始值。 */
uint32_t PressureSensorProto_EncodePaToRaw(float pa);

/** 编码 32 位小端值。 */
esp_err_t PressureSensorProto_EncodeU32LE(uint32_t value, uint8_t out_buf[4]);

/** 编码报警参数。 */
esp_err_t PressureSensorProto_EncodeAlarmSettings(const PressureSensorAlarmConfig_t *alarm,
										 uint8_t out_buf[12]);

/** 编码工作模式。 */
esp_err_t PressureSensorProto_EncodeWorkMode(PressureSensorWorkMode_t mode,
										 uint8_t *out_buf,
										 uint16_t *out_len);

#ifdef __cplusplus
}
#endif

#endif
