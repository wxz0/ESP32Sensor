#include <string.h>

#include "PressureSensorProtocol.h"

static const esp_bt_uuid_t s_service_uuid = {
	.len = ESP_UUID_LEN_128,
	.uuid = {.uuid128 = {0x9b, 0x97, 0x59, 0x69, 0xba, 0x44, 0xf2, 0xab,
						 0xcb, 0x47, 0xa9, 0x9d, 0x23, 0x15, 0x47, 0xdf}},
};

static const esp_bt_uuid_t s_pressure_char_uuid = {
	.len = ESP_UUID_LEN_128,
	.uuid = {.uuid128 = {0x9b, 0x97, 0x59, 0x69, 0xba, 0x44, 0xf2, 0xab,
						 0xcb, 0x47, 0xa9, 0x9d, 0x24, 0x15, 0x47, 0xdf}},
};

static const esp_bt_uuid_t s_sample_char_uuid = {
	.len = ESP_UUID_LEN_128,
	.uuid = {.uuid128 = {0x9b, 0x97, 0x59, 0x69, 0xba, 0x44, 0xf2, 0xab,
						 0xcb, 0x47, 0xa9, 0x9d, 0x25, 0x15, 0x47, 0xdf}},
};

static const esp_bt_uuid_t s_report_char_uuid = {
	.len = ESP_UUID_LEN_128,
	.uuid = {.uuid128 = {0x9b, 0x97, 0x59, 0x69, 0xba, 0x44, 0xf2, 0xab,
						 0xcb, 0x47, 0xa9, 0x9d, 0x27, 0x15, 0x47, 0xdf}},
};

static const esp_bt_uuid_t s_alarm_char_uuid = {
	.len = ESP_UUID_LEN_128,
	.uuid = {.uuid128 = {0x9b, 0x97, 0x59, 0x69, 0xba, 0x44, 0xf2, 0xab,
						 0xcb, 0x47, 0xa9, 0x9d, 0x28, 0x15, 0x47, 0xdf}},
};

static const esp_bt_uuid_t s_work_mode_char_uuid = {
	.len = ESP_UUID_LEN_128,
	.uuid = {.uuid128 = {0x9b, 0x97, 0x59, 0x69, 0xba, 0x44, 0xf2, 0xab,
						 0xcb, 0x47, 0xa9, 0x9d, 0x29, 0x15, 0x47, 0xdf}},
};

/* 获取服务 UUID。 */
const esp_bt_uuid_t *PressureSensorProto_GetServiceUuid(void)
{
	return &s_service_uuid;
}

/* 获取压力数据特征 UUID。 */
const esp_bt_uuid_t *PressureSensorProto_GetPressureCharUuid(void)
{
	return &s_pressure_char_uuid;
}

/* 获取采集周期特征 UUID。 */
const esp_bt_uuid_t *PressureSensorProto_GetSamplePeriodCharUuid(void)
{
	return &s_sample_char_uuid;
}

/* 获取上报周期特征 UUID。 */
const esp_bt_uuid_t *PressureSensorProto_GetReportPeriodCharUuid(void)
{
	return &s_report_char_uuid;
}

/* 获取报警参数特征 UUID。 */
const esp_bt_uuid_t *PressureSensorProto_GetAlarmSettingsCharUuid(void)
{
	return &s_alarm_char_uuid;
}

/* 获取工作模式特征 UUID。 */
const esp_bt_uuid_t *PressureSensorProto_GetWorkModeCharUuid(void)
{
	return &s_work_mode_char_uuid;
}

/* 读取小端 32 位整数。 */
static uint32_t read_le_u32(const uint8_t *p)
{
	return ((uint32_t)p[0]) |
		   ((uint32_t)p[1] << 8U) |
		   ((uint32_t)p[2] << 16U) |
		   ((uint32_t)p[3] << 24U);
}

/* 写入小端 32 位整数。 */
static void write_le_u32(uint32_t value, uint8_t *out)
{
	out[0] = (uint8_t)(value & 0xFFU);
	out[1] = (uint8_t)((value >> 8U) & 0xFFU);
	out[2] = (uint8_t)((value >> 16U) & 0xFFU);
	out[3] = (uint8_t)((value >> 24U) & 0xFFU);
}

/* 解析压力数值 payload。 */
esp_err_t PressureSensorProto_ParsePressureData(const uint8_t *value,
											 uint16_t value_len,
											 PressureSensorState_t *state)
{
	if (value == NULL || state == NULL || value_len < 14U) {
		return ESP_ERR_INVALID_ARG;
	}

	state->device_type = value[0];
	state->alarm_status = value[1];
	state->pressure.pressure_cur = (float)read_le_u32(&value[2]) * 0.1f;
	state->pressure.pressure_min = (float)read_le_u32(&value[6]) * 0.1f;
	state->pressure.pressure_max = (float)read_le_u32(&value[10]) * 0.1f;
	state->data_valid = true;
	return ESP_OK;
}

/* 解析周期元数据 payload。 */
esp_err_t PressureSensorProto_ParsePeriodMeta(const uint8_t *value,
											 uint16_t value_len,
											 uint32_t *current_ms,
											 uint32_t *min_ms,
											 uint32_t *max_ms)
{
	if (value == NULL || current_ms == NULL || min_ms == NULL || max_ms == NULL || value_len < 12U) {
		return ESP_ERR_INVALID_ARG;
	}

	*current_ms = read_le_u32(&value[0]);
	*min_ms = read_le_u32(&value[4]);
	*max_ms = read_le_u32(&value[8]);
	return ESP_OK;
}

/* 解析报警参数 payload。 */
esp_err_t PressureSensorProto_ParseAlarmSettings(const uint8_t *value,
											 uint16_t value_len,
											 PressureSensorAlarmConfig_t *alarm)
{
	if (value == NULL || alarm == NULL || value_len < 12U) {
		return ESP_ERR_INVALID_ARG;
	}

	alarm->low_alarm_pa = (float)read_le_u32(&value[0]) * 0.1f;
	alarm->high_alarm_pa = (float)read_le_u32(&value[4]) * 0.1f;
	alarm->oscillation_alarm_pa = (float)read_le_u32(&value[8]) * 0.1f;
	return ESP_OK;
}

/* 解析工作模式 payload。 */
esp_err_t PressureSensorProto_ParseWorkMode(const uint8_t *value,
										 uint16_t value_len,
										 PressureSensorWorkMode_t *mode)
{
	if (value == NULL || mode == NULL || value_len < 1U) {
		return ESP_ERR_INVALID_ARG;
	}

	*mode = (PressureSensorWorkMode_t)value[0];
	return ESP_OK;
}

/* 将 Pa 转为协议原始值。 */
uint32_t PressureSensorProto_EncodePaToRaw(float pa)
{
	if (pa <= 0.0f) {
		return 0U;
	}

	return (uint32_t)(pa * 10.0f + 0.5f);
}

/* 编码 32 位小端值。 */
esp_err_t PressureSensorProto_EncodeU32LE(uint32_t value, uint8_t out_buf[4])
{
	if (out_buf == NULL) {
		return ESP_ERR_INVALID_ARG;
	}

	write_le_u32(value, out_buf);
	return ESP_OK;
}

/* 编码报警参数。 */
esp_err_t PressureSensorProto_EncodeAlarmSettings(const PressureSensorAlarmConfig_t *alarm,
										 uint8_t out_buf[12])
{
	if (alarm == NULL || out_buf == NULL) {
		return ESP_ERR_INVALID_ARG;
	}

	write_le_u32(PressureSensorProto_EncodePaToRaw(alarm->low_alarm_pa), &out_buf[0]);
	write_le_u32(PressureSensorProto_EncodePaToRaw(alarm->high_alarm_pa), &out_buf[4]);
	write_le_u32(PressureSensorProto_EncodePaToRaw(alarm->oscillation_alarm_pa), &out_buf[8]);
	return ESP_OK;
}

/* 编码工作模式。 */
esp_err_t PressureSensorProto_EncodeWorkMode(PressureSensorWorkMode_t mode,
										 uint8_t *out_buf,
										 uint16_t *out_len)
{
	if (out_buf == NULL || out_len == NULL) {
		return ESP_ERR_INVALID_ARG;
	}

	out_buf[0] = (uint8_t)mode;
	*out_len = 1U;
	return ESP_OK;
}
