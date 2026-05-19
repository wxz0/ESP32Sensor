#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "PressureSensor.h"
#include "PressureSensorProtocol.h"

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"

#ifndef CONFIG_PRESSURE_SENSOR_BLE_POLL_INTERVAL_MS
#define CONFIG_PRESSURE_SENSOR_BLE_POLL_INTERVAL_MS 1000
#endif

#ifndef CONFIG_PRESSURE_SENSOR1_BLE_ADDR
#define CONFIG_PRESSURE_SENSOR1_BLE_ADDR ""
#endif

#ifndef CONFIG_PRESSURE_SENSOR2_BLE_ADDR
#define CONFIG_PRESSURE_SENSOR2_BLE_ADDR ""
#endif

typedef struct {
	bool addr_configured;
	uint8_t sensor_index;
	esp_bd_addr_t addr;
	bool connected;
	bool connecting;
	bool data_valid;
	bool config_pending;
	uint8_t addr_type;
	uint16_t conn_id;
	uint16_t service_start_handle;
	uint16_t service_end_handle;
	uint16_t pressure_char_handle;
	uint16_t pressure_cccd_handle;
	uint16_t sample_char_handle;
	uint16_t report_char_handle;
	uint16_t alarm_char_handle;
	uint16_t work_mode_char_handle;
	PressureSensorConfigRequest_t desired_config;
	PressureSensorState_t state;
} pressure_sensor_link_t;

typedef struct {
	PressureSensorDataCallback_t callback;
	void *user_ctx;
} pressure_sensor_data_listener_t;

static const char *TAG = "PressureSensor";
static pressure_sensor_link_t s_links[PRESSURE_SENSOR_COUNT] = {0};
static pressure_sensor_data_listener_t s_data_listener = {0};
static SemaphoreHandle_t s_lock = NULL;
static esp_gatt_if_t s_gattc_if = ESP_GATT_IF_NONE;
static bool s_scan_param_ready = false;
static bool s_ble_ready = false;
static TaskHandle_t s_worker_task = NULL;

static esp_ble_scan_params_t s_scan_params = {
	.scan_type = BLE_SCAN_TYPE_ACTIVE,
	.own_addr_type = BLE_ADDR_TYPE_PUBLIC,
	.scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
	.scan_interval = 0x50,
	.scan_window = 0x30,
	.scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE,
};

/* 将 MAC 字符串解析为 ESP32 蓝牙地址。 */
static bool parse_mac_str(const char *mac_str, esp_bd_addr_t out_addr)
{
	unsigned int v[6] = {0};

	if (mac_str == NULL || out_addr == NULL) {
		return false;
	}
	if (strlen(mac_str) < 11U) {
		return false;
	}
	if (sscanf(mac_str, "%2x:%2x:%2x:%2x:%2x:%2x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) {
		return false;
	}
	for (size_t i = 0; i < 6; i++) {
		out_addr[i] = (uint8_t)v[i];
	}
	return true;
}

/* 按地址查找传感器链接。 */
static int find_link_by_addr(const esp_bd_addr_t bda)
{
	for (int i = 0; i < PRESSURE_SENSOR_COUNT; i++) {
		if (s_links[i].addr_configured && memcmp(s_links[i].addr, bda, sizeof(esp_bd_addr_t)) == 0) {
			return i;
		}
	}
	return -1;
}

/* 按连接 ID 查找传感器链接。 */
static int find_link_by_conn(uint16_t conn_id)
{
	for (int i = 0; i < PRESSURE_SENSOR_COUNT; i++) {
		if (s_links[i].connected && s_links[i].conn_id == conn_id) {
			return i;
		}
	}
	return -1;
}

/* 从期望配置同步部分状态字段。 */
static void sync_state_from_desired(pressure_sensor_link_t *link)
{
	if (link == NULL) {
		return;
	}

	if (link->desired_config.set_work_mode) {
		link->state.work_mode = link->desired_config.work_mode;
	}
}

/* 初始化默认配置为网关模式。 */
static void init_default_desired_config(pressure_sensor_link_t *link)
{
	if (link == NULL) 
	{
		return;
	}

	memset(&link->desired_config, 0, sizeof(link->desired_config));
	link->desired_config.set_work_mode = true;
	link->desired_config.work_mode = PRESSURE_SENSOR_WORK_MODE_GATEWAY;
	link->config_pending = true;
	sync_state_from_desired(link);
}

/* 解析 notify/read 到达的压力数据。 */
static void parse_pressure_payload(pressure_sensor_link_t *link, const uint8_t *value, uint16_t value_len)
{
	if (link == NULL || value == NULL || value_len < 14U) {
		return;
	}

	PressureSensorProto_ParsePressureData(value, value_len, &link->state);
	link->data_valid = true;
	link->state.connected = true;

	if (s_data_listener.callback != NULL) {
		s_data_listener.callback(link->sensor_index, &link->state.pressure, s_data_listener.user_ctx);
	}
}

/* 解析采集/上报周期元数据。 */
static void parse_period_payload(pressure_sensor_link_t *link,
								 const uint8_t *value,
								 uint16_t value_len,
								 bool is_sample)
{
	uint32_t current_ms = 0;
	uint32_t min_ms = 0;
	uint32_t max_ms = 0;

	if (link == NULL || value == NULL || value_len < 12U) {
		return;
	}

	if (PressureSensorProto_ParsePeriodMeta(value, value_len, &current_ms, &min_ms, &max_ms) != ESP_OK) {
		return;
	}

	if (is_sample) {
		link->state.sample_period_ms = current_ms;
		link->state.sample_period_min_ms = min_ms;
		link->state.sample_period_max_ms = max_ms;
	} else {
		link->state.report_period_ms = current_ms;
		link->state.report_period_min_ms = min_ms;
		link->state.report_period_max_ms = max_ms;
	}
	link->state.config_valid = true;
}

/* 解析报警参数。 */
static void parse_alarm_payload(pressure_sensor_link_t *link, const uint8_t *value, uint16_t value_len)
{
	if (link == NULL || value == NULL || value_len < 12U) {
		return;
	}

	if (PressureSensorProto_ParseAlarmSettings(value, value_len, &link->state.alarm) == ESP_OK) {
		link->state.config_valid = true;
	}
}

/* 解析工作模式。 */
static void parse_work_mode_payload(pressure_sensor_link_t *link, const uint8_t *value, uint16_t value_len)
{
	PressureSensorWorkMode_t mode;

	if (link == NULL || value == NULL || value_len < 1U) {
		return;
	}

	if (PressureSensorProto_ParseWorkMode(value, value_len, &mode) == ESP_OK) {
		link->state.work_mode = mode;
		link->state.config_valid = true;
	}
}

/* 尝试重新启动扫描。 */
static void try_start_scan(void)
{
	if (s_ble_ready && s_scan_param_ready && s_gattc_if != ESP_GATT_IF_NONE) {
		esp_err_t err = esp_ble_gap_start_scanning(0);
		if (err != ESP_OK) {
			ESP_LOGW(TAG, "Start scan failed: %s", esp_err_to_name(err));
		}
	}
}

/* 从服务缓存中更新各特征句柄。 */
static void update_char_handles(pressure_sensor_link_t *link)
{
	uint16_t count = 0;
	esp_gattc_char_elem_t *result = NULL;
	esp_gattc_descr_elem_t *descr_result = NULL;

	if (link == NULL || link->service_start_handle == 0U || link->service_end_handle == 0U) {
		return;
	}

	ESP_LOGI(TAG,
			 "Discovering chars for sensor[%u], conn_id=%u, service=[0x%04x-0x%04x]",
			 link->sensor_index,
			 link->conn_id,
			 link->service_start_handle,
			 link->service_end_handle);

	if (esp_ble_gattc_get_attr_count(s_gattc_if,
									 link->conn_id,
									 ESP_GATT_DB_CHARACTERISTIC,
									 link->service_start_handle,
									 link->service_end_handle,
									 ESP_GATT_ILLEGAL_HANDLE,
									 &count) != ESP_OK || count == 0U) {
		return;
	}

	result = calloc(count, sizeof(esp_gattc_char_elem_t));
	if (result == NULL) {
		return;
	}

	uint16_t tmp_count = count;
	if (esp_ble_gattc_get_char_by_uuid(s_gattc_if,
									   link->conn_id,
									   link->service_start_handle,
									   link->service_end_handle,
									   *(PressureSensorProto_GetPressureCharUuid()),
									   result,
									   &tmp_count) == ESP_GATT_OK && tmp_count > 0U) {
		link->pressure_char_handle = result[0].char_handle;
		link->pressure_cccd_handle = 0U;

		uint16_t descr_count = 0;
		esp_bt_uuid_t cccd_uuid = {
			.len = ESP_UUID_LEN_16,
			.uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG},
		};
		if (esp_ble_gattc_get_attr_count(s_gattc_if,
										 link->conn_id,
										 ESP_GATT_DB_DESCRIPTOR,
										 link->service_start_handle,
										 link->service_end_handle,
										 link->pressure_char_handle,
										 &descr_count) == ESP_OK && descr_count > 0U) {
			descr_result = calloc(descr_count, sizeof(esp_gattc_descr_elem_t));
			if (descr_result != NULL) {
				uint16_t descr_tmp = descr_count;
				if (esp_ble_gattc_get_descr_by_char_handle(s_gattc_if,
														 link->conn_id,
														 link->pressure_char_handle,
														 cccd_uuid,
														 descr_result,
														 &descr_tmp) == ESP_GATT_OK && descr_tmp > 0U) {
					link->pressure_cccd_handle = descr_result[0].handle;
				}
				free(descr_result);
			}
		}
	}

	tmp_count = count;
	if (esp_ble_gattc_get_char_by_uuid(s_gattc_if,
									   link->conn_id,
									   link->service_start_handle,
									   link->service_end_handle,
									   *(PressureSensorProto_GetSamplePeriodCharUuid()),
									   result,
									   &tmp_count) == ESP_GATT_OK && tmp_count > 0U) {
		link->sample_char_handle = result[0].char_handle;
	}

	tmp_count = count;
	if (esp_ble_gattc_get_char_by_uuid(s_gattc_if,
									   link->conn_id,
									   link->service_start_handle,
									   link->service_end_handle,
									   *(PressureSensorProto_GetReportPeriodCharUuid()),
									   result,
									   &tmp_count) == ESP_GATT_OK && tmp_count > 0U) {
		link->report_char_handle = result[0].char_handle;
	}

	tmp_count = count;
	if (esp_ble_gattc_get_char_by_uuid(s_gattc_if,
									   link->conn_id,
									   link->service_start_handle,
									   link->service_end_handle,
									   *(PressureSensorProto_GetAlarmSettingsCharUuid()),
									   result,
									   &tmp_count) == ESP_GATT_OK && tmp_count > 0U) {
		link->alarm_char_handle = result[0].char_handle;
	}

	tmp_count = count;
	if (esp_ble_gattc_get_char_by_uuid(s_gattc_if,
									   link->conn_id,
									   link->service_start_handle,
									   link->service_end_handle,
									   *(PressureSensorProto_GetWorkModeCharUuid()),
									   result,
									   &tmp_count) == ESP_GATT_OK && tmp_count > 0U) {
		link->work_mode_char_handle = result[0].char_handle;
	}

	ESP_LOGI(TAG,
			 "Sensor[%u] handles: pressure=0x%04x cccd=0x%04x sample=0x%04x report=0x%04x alarm=0x%04x mode=0x%04x",
			 link->sensor_index,
			 link->pressure_char_handle,
			 link->pressure_cccd_handle,
			 link->sample_char_handle,
			 link->report_char_handle,
			 link->alarm_char_handle,
			 link->work_mode_char_handle);

	free(result);
}

/* 写入 32 位小端特征值。 */
static esp_err_t write_u32_char(uint16_t conn_id, uint16_t handle, uint32_t value)
{
	uint8_t buf[4];

	if (handle == 0U) {
		return ESP_ERR_INVALID_STATE;
	}

	PressureSensorProto_EncodeU32LE(value, buf);
	return esp_ble_gattc_write_char(s_gattc_if,
									 conn_id,
									 handle,
									 sizeof(buf),
									 buf,
									 ESP_GATT_WRITE_TYPE_RSP,
									 ESP_GATT_AUTH_REQ_NONE);
}

				/* 写入工作模式特征。 */
static esp_err_t write_work_mode(pressure_sensor_link_t *link)
{
	uint8_t buf[1];
	uint16_t len = 0;

	if (link == NULL || link->work_mode_char_handle == 0U) {
		return ESP_ERR_INVALID_STATE;
	}

	PressureSensorProto_EncodeWorkMode(link->desired_config.work_mode, buf, &len);
	return esp_ble_gattc_write_char(s_gattc_if,
									 link->conn_id,
									 link->work_mode_char_handle,
									 len,
									 buf,
									 ESP_GATT_WRITE_TYPE_RSP,
									 ESP_GATT_AUTH_REQ_NONE);
}

				/* 写入报警参数特征。 */
static esp_err_t write_alarm_settings(pressure_sensor_link_t *link)
{
	uint8_t buf[12];

	if (link == NULL || link->alarm_char_handle == 0U) {
		return ESP_ERR_INVALID_STATE;
	}

	if (PressureSensorProto_EncodeAlarmSettings(&link->desired_config.alarm, buf) != ESP_OK) {
		return ESP_ERR_INVALID_ARG;
	}

	return esp_ble_gattc_write_char(s_gattc_if,
									 link->conn_id,
									 link->alarm_char_handle,
									 sizeof(buf),
									 buf,
									 ESP_GATT_WRITE_TYPE_RSP,
									 ESP_GATT_AUTH_REQ_NONE);
}

				/* 写入采集周期特征。 */
static esp_err_t write_sample_period(pressure_sensor_link_t *link)
{
	return write_u32_char(link->conn_id, link->sample_char_handle, link->desired_config.sample_period_ms);
}

				/* 写入上报周期特征。 */
static esp_err_t write_report_period(pressure_sensor_link_t *link)
{
	return write_u32_char(link->conn_id, link->report_char_handle, link->desired_config.report_period_ms);
}

				/* 执行挂起的配置下发。 */
static void apply_pending_config(pressure_sensor_link_t *link)
{
	esp_err_t err;

	if (link == NULL || !link->connected) {
		return;
	}

	if (link->desired_config.set_work_mode && link->work_mode_char_handle != 0U) {
		err = write_work_mode(link);
		if (err == ESP_OK) {
			ESP_LOGI(TAG, "Sensor[%u] write work_mode=%u", link->sensor_index, (unsigned)link->desired_config.work_mode);
			link->state.work_mode = link->desired_config.work_mode;
			link->desired_config.set_work_mode = false;
		} else {
			ESP_LOGW(TAG, "Sensor[%u] write work_mode failed: %s", link->sensor_index, esp_err_to_name(err));
		}
		return;
	}

	if (link->desired_config.set_sample_period_ms && link->sample_char_handle != 0U) {
		err = write_sample_period(link);
		if (err == ESP_OK) {
			ESP_LOGI(TAG, "Sensor[%u] write sample_period_ms=%lu", link->sensor_index, (unsigned long)link->desired_config.sample_period_ms);
			link->state.sample_period_ms = link->desired_config.sample_period_ms;
			link->desired_config.set_sample_period_ms = false;
		} else {
			ESP_LOGW(TAG, "Sensor[%u] write sample_period failed: %s", link->sensor_index, esp_err_to_name(err));
		}
		return;
	}

	if (link->desired_config.set_report_period_ms && link->report_char_handle != 0U) {
		err = write_report_period(link);
		if (err == ESP_OK) {
			ESP_LOGI(TAG, "Sensor[%u] write report_period_ms=%lu", link->sensor_index, (unsigned long)link->desired_config.report_period_ms);
			link->state.report_period_ms = link->desired_config.report_period_ms;
			link->desired_config.set_report_period_ms = false;
		} else {
			ESP_LOGW(TAG, "Sensor[%u] write report_period failed: %s", link->sensor_index, esp_err_to_name(err));
		}
		return;
	}

	if (link->desired_config.set_alarm && link->alarm_char_handle != 0U) {
		err = write_alarm_settings(link);
		if (err == ESP_OK) {
			ESP_LOGI(TAG,
					 "Sensor[%u] write alarm high=%f low=%f",
					 link->sensor_index,
					 (double)link->desired_config.alarm.high_alarm_pa,
					 (double)link->desired_config.alarm.low_alarm_pa);
			link->state.alarm = link->desired_config.alarm;
			link->desired_config.set_alarm = false;
		} else {
			ESP_LOGW(TAG, "Sensor[%u] write alarm failed: %s", link->sensor_index, esp_err_to_name(err));
		}
		return;
	}
}

/* 后台轮询任务，负责连接维持和配置下发。 */
static void poll_task(void *arg)
{
	(void)arg;

	while (1) {
		if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(50)) == pdTRUE) {
			for (int i = 0; i < PRESSURE_SENSOR_COUNT; i++) {
				pressure_sensor_link_t *link = &s_links[i];

				if (!link->connected || link->conn_id == 0U) {
					continue;
				}

				apply_pending_config(link);

				if (link->pressure_char_handle != 0U) {
					/* 压力数据改为由 notify 触发读取，不再轮询读取 */
				}

				if (link->sample_char_handle != 0U && !link->state.config_valid) {
					esp_ble_gattc_read_char(s_gattc_if,
											link->conn_id,
											link->sample_char_handle,
											ESP_GATT_AUTH_REQ_NONE);
				}

				if (link->report_char_handle != 0U && !link->state.config_valid) {
					esp_ble_gattc_read_char(s_gattc_if,
											link->conn_id,
											link->report_char_handle,
											ESP_GATT_AUTH_REQ_NONE);
				}

				if (link->alarm_char_handle != 0U && !link->state.config_valid) {
					esp_ble_gattc_read_char(s_gattc_if,
											link->conn_id,
											link->alarm_char_handle,
											ESP_GATT_AUTH_REQ_NONE);
				}

				if (link->work_mode_char_handle != 0U && !link->state.config_valid) {
					esp_ble_gattc_read_char(s_gattc_if,
											link->conn_id,
											link->work_mode_char_handle,
											ESP_GATT_AUTH_REQ_NONE);
				}
			}
			xSemaphoreGive(s_lock);
		}

		vTaskDelay(pdMS_TO_TICKS(CONFIG_PRESSURE_SENSOR_BLE_POLL_INTERVAL_MS));
	}
}

/* GAP 事件回调。 */
static void gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
	switch (event) {
	case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
		s_scan_param_ready = true;
		ESP_LOGI(TAG, "Scan parameters set, ready to start scanning");
		try_start_scan();
		break;

	case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
		if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) 
		{
			ESP_LOGE(TAG, "Scan start failed: %d", param->scan_start_cmpl.status);
		}
		break;

	case ESP_GAP_BLE_SCAN_RESULT_EVT:
		if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
			if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(20)) == pdTRUE) {
				int idx = find_link_by_addr(param->scan_rst.bda);
				if (idx >= 0 && !s_links[idx].connected && !s_links[idx].connecting && s_gattc_if != ESP_GATT_IF_NONE) {
					ESP_LOGI(TAG,
							 "Found sensor[%d], addr=%02x:%02x:%02x:%02x:%02x:%02x, addr_type=%u. Opening...",
							 idx,
							 param->scan_rst.bda[0],
							 param->scan_rst.bda[1],
							 param->scan_rst.bda[2],
							 param->scan_rst.bda[3],
							 param->scan_rst.bda[4],
							 param->scan_rst.bda[5],
							 (unsigned)param->scan_rst.ble_addr_type);
					s_links[idx].connecting = true;
					s_links[idx].addr_type = param->scan_rst.ble_addr_type;
					esp_ble_gap_stop_scanning();
					esp_ble_gattc_open(s_gattc_if, s_links[idx].addr, s_links[idx].addr_type, true);
				}
				xSemaphoreGive(s_lock);
			}
		} else if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_CMPL_EVT) {
			try_start_scan();
		}
		break;

	default:
		break;
	}
}

/* GATTC 事件回调。 */
static void gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
	switch (event) {
	case ESP_GATTC_REG_EVT:
		if (param->reg.status == ESP_GATT_OK) {
			s_gattc_if = gattc_if;
			s_ble_ready = true;
			try_start_scan();
		}
		break;

	case ESP_GATTC_OPEN_EVT: {
		if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(50)) == pdTRUE) {
			int idx = find_link_by_addr(param->open.remote_bda);
			if (idx >= 0) {
				pressure_sensor_link_t *link = &s_links[idx];
				link->connecting = false;
				if (param->open.status == ESP_GATT_OK) {
					ESP_LOGI(TAG,
							 "Sensor[%d] connected, conn_id=%u, mtu=%u",
							 idx,
							 param->open.conn_id,
							 param->open.mtu);
					link->connected = true;
					link->state.connected = true;
					link->state.data_valid = false;
					link->conn_id = param->open.conn_id;
					link->service_start_handle = 0U;
					link->service_end_handle = 0U;
					link->pressure_char_handle = 0U;
					link->sample_char_handle = 0U;
					link->report_char_handle = 0U;
					link->alarm_char_handle = 0U;
					link->work_mode_char_handle = 0U;
					esp_ble_gattc_search_service(gattc_if, link->conn_id, PressureSensorProto_GetServiceUuid());
				} else {
					ESP_LOGW(TAG, "Sensor[%d] open failed, status=%d", idx, param->open.status);
					link->state.connected = false;
					try_start_scan();
				}
			}
			xSemaphoreGive(s_lock);
		}
		break;
	}

	case ESP_GATTC_SEARCH_RES_EVT: {
		if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(50)) == pdTRUE) {
			int idx = find_link_by_conn(param->search_res.conn_id);
			if (idx >= 0 && param->search_res.srvc_id.uuid.len == ESP_UUID_LEN_128 &&
				memcmp(param->search_res.srvc_id.uuid.uuid.uuid128,
					   PressureSensorProto_GetServiceUuid()->uuid.uuid128,
					   ESP_UUID_LEN_128) == 0) {
				s_links[idx].service_start_handle = param->search_res.start_handle;
				s_links[idx].service_end_handle = param->search_res.end_handle;
				ESP_LOGI(TAG,
						 "Sensor[%d] service found: [0x%04x-0x%04x]",
						 idx,
						 param->search_res.start_handle,
						 param->search_res.end_handle);
			}
			xSemaphoreGive(s_lock);
		}
		break;
	}

	case ESP_GATTC_SEARCH_CMPL_EVT: {
		if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(50)) == pdTRUE) {
			int idx = find_link_by_conn(param->search_cmpl.conn_id);
			if (idx >= 0) {
				ESP_LOGI(TAG, "Sensor[%d] service discovery complete, status=%d", idx, param->search_cmpl.status);
				update_char_handles(&s_links[idx]);
				if (s_links[idx].pressure_char_handle != 0U) {
					esp_ble_gattc_register_for_notify(gattc_if,
													 s_links[idx].addr,
													 s_links[idx].pressure_char_handle);
				} else {
					ESP_LOGW(TAG, "Sensor[%d] pressure characteristic not found", idx);
				}
				if (s_links[idx].pressure_char_handle != 0U) {
					esp_ble_gattc_read_char(gattc_if,
											s_links[idx].conn_id,
											s_links[idx].pressure_char_handle,
											ESP_GATT_AUTH_REQ_NONE);
				}
				if (s_links[idx].sample_char_handle != 0U) {
					esp_ble_gattc_read_char(gattc_if,
											s_links[idx].conn_id,
											s_links[idx].sample_char_handle,
											ESP_GATT_AUTH_REQ_NONE);
				}
				if (s_links[idx].report_char_handle != 0U) {
					esp_ble_gattc_read_char(gattc_if,
											s_links[idx].conn_id,
											s_links[idx].report_char_handle,
											ESP_GATT_AUTH_REQ_NONE);
				}
				if (s_links[idx].alarm_char_handle != 0U) {
					esp_ble_gattc_read_char(gattc_if,
											s_links[idx].conn_id,
											s_links[idx].alarm_char_handle,
											ESP_GATT_AUTH_REQ_NONE);
				}
				if (s_links[idx].work_mode_char_handle != 0U) {
					esp_ble_gattc_read_char(gattc_if,
											s_links[idx].conn_id,
											s_links[idx].work_mode_char_handle,
											ESP_GATT_AUTH_REQ_NONE);
				}
			}
			xSemaphoreGive(s_lock);
		}
		try_start_scan();
		break;
	}

	case ESP_GATTC_READ_CHAR_EVT: {
		if (param->read.status == ESP_GATT_OK && xSemaphoreTake(s_lock, pdMS_TO_TICKS(50)) == pdTRUE) {
			int idx = find_link_by_conn(param->read.conn_id);
			if (idx >= 0) {
				pressure_sensor_link_t *link = &s_links[idx];
				if (param->read.handle == link->pressure_char_handle) {
					parse_pressure_payload(link, param->read.value, param->read.value_len);
				} else if (param->read.handle == link->sample_char_handle) {
					parse_period_payload(link, param->read.value, param->read.value_len, true);
				} else if (param->read.handle == link->report_char_handle) {
					parse_period_payload(link, param->read.value, param->read.value_len, false);
				} else if (param->read.handle == link->alarm_char_handle) {
					parse_alarm_payload(link, param->read.value, param->read.value_len);
				} else if (param->read.handle == link->work_mode_char_handle) {
					parse_work_mode_payload(link, param->read.value, param->read.value_len);
				}
			}
			xSemaphoreGive(s_lock);
		} else if (param->read.status != ESP_GATT_OK) {
			ESP_LOGW(TAG,
					 "Read char failed: conn_id=%u handle=0x%04x status=%d",
					 param->read.conn_id,
					 param->read.handle,
					 param->read.status);
		}
		break;
	}

	case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
		if (param->reg_for_notify.status == ESP_GATT_OK && xSemaphoreTake(s_lock, pdMS_TO_TICKS(50)) == pdTRUE) {
			for (int i = 0; i < PRESSURE_SENSOR_COUNT; i++) {
				pressure_sensor_link_t *link = &s_links[i];
				if (param->reg_for_notify.handle == link->pressure_char_handle && link->pressure_cccd_handle != 0U) {
					uint16_t notify_en = 1U;
					ESP_LOGI(TAG,
							 "Sensor[%d] enabling notify via CCCD=0x%04x",
							 i,
							 link->pressure_cccd_handle);
					esp_ble_gattc_write_char_descr(gattc_if,
													  link->conn_id,
													  link->pressure_cccd_handle,
													  sizeof(notify_en),
													  (uint8_t *)&notify_en,
													  ESP_GATT_WRITE_TYPE_RSP,
													  ESP_GATT_AUTH_REQ_NONE);
				}
			}
			xSemaphoreGive(s_lock);
		} else {
			ESP_LOGW(TAG, "Register notify failed: status=%d", param->reg_for_notify.status);
		}
		break;
	}

	case ESP_GATTC_NOTIFY_EVT: {
		if (param->notify.is_notify && xSemaphoreTake(s_lock, pdMS_TO_TICKS(50)) == pdTRUE) {
			int idx = find_link_by_conn(param->notify.conn_id);
			if (idx >= 0) {
				pressure_sensor_link_t *link = &s_links[idx];
				if (param->notify.handle == link->pressure_char_handle) {
					ESP_LOGD(TAG, "Sensor[%d] notify received, triggering read", idx);
					esp_ble_gattc_read_char(gattc_if,
											link->conn_id,
											link->pressure_char_handle,
											ESP_GATT_AUTH_REQ_NONE);
				}
			}
			xSemaphoreGive(s_lock);
		}
		break;
	}

	case ESP_GATTC_DISCONNECT_EVT: {
		ESP_LOGW(TAG,
				 "Disconnected: addr=%02x:%02x:%02x:%02x:%02x:%02x reason=0x%02x",
				 param->disconnect.remote_bda[0],
				 param->disconnect.remote_bda[1],
				 param->disconnect.remote_bda[2],
				 param->disconnect.remote_bda[3],
				 param->disconnect.remote_bda[4],
				 param->disconnect.remote_bda[5],
				 param->disconnect.reason);
		if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(50)) == pdTRUE) {
			int idx = find_link_by_addr(param->disconnect.remote_bda);
			if (idx >= 0) {
				pressure_sensor_link_t *link = &s_links[idx];
				link->connected = false;
				link->connecting = false;
				link->data_valid = false;
				link->conn_id = 0U;
				link->service_start_handle = 0U;
				link->service_end_handle = 0U;
				link->pressure_char_handle = 0U;
				link->pressure_cccd_handle = 0U;
				link->sample_char_handle = 0U;
				link->report_char_handle = 0U;
				link->alarm_char_handle = 0U;
				link->work_mode_char_handle = 0U;
				link->state.connected = false;
				link->state.data_valid = false;
			}
			xSemaphoreGive(s_lock);
		}
		try_start_scan();
		break;
	}

	default:
		break;
	}
}

/* 应用一组压力传感器配置请求。 */
static esp_err_t apply_config_request(pressure_sensor_link_t *link, const PressureSensorConfigRequest_t *config)
{
	if (link == NULL || config == NULL) {
		return ESP_ERR_INVALID_ARG;
	}

	if (config->set_work_mode) {
		link->desired_config.set_work_mode = true;
		link->desired_config.work_mode = config->work_mode;
		link->config_pending = true;
		ESP_LOGI(TAG, "Sensor[%u] queue work_mode=%u", link->sensor_index, (unsigned)config->work_mode);
	}

	if (config->set_sample_period_ms) {
		link->desired_config.set_sample_period_ms = true;
		link->desired_config.sample_period_ms = config->sample_period_ms;
		link->config_pending = true;
		ESP_LOGI(TAG, "Sensor[%u] queue sample_period_ms=%lu", link->sensor_index, (unsigned long)config->sample_period_ms);
	}

	if (config->set_report_period_ms) {
		link->desired_config.set_report_period_ms = true;
		link->desired_config.report_period_ms = config->report_period_ms;
		link->config_pending = true;
		ESP_LOGI(TAG, "Sensor[%u] queue report_period_ms=%lu", link->sensor_index, (unsigned long)config->report_period_ms);
	}

	if (config->set_alarm) {
		link->desired_config.set_alarm = true;
		link->desired_config.alarm = config->alarm;
		link->config_pending = true;
		ESP_LOGI(TAG,
				 "Sensor[%u] queue alarm high=%f low=%f",
				 link->sensor_index,
				 (double)config->alarm.high_alarm_pa,
				 (double)config->alarm.low_alarm_pa);
	}

	sync_state_from_desired(link);
	return ESP_OK;
}

/* 初始化压力传感器 BLE 客户端。 */
esp_err_t PressureSensor_Init(void)
{
	esp_err_t err = ESP_OK;

	if (s_lock == NULL) {
		s_lock = xSemaphoreCreateMutex();
		if (s_lock == NULL) {
			return ESP_ERR_NO_MEM;
		}
	}

	memset(s_links, 0, sizeof(s_links));

	if (!parse_mac_str(CONFIG_PRESSURE_SENSOR1_BLE_ADDR, s_links[0].addr)) {
		ESP_LOGW(TAG, "Sensor1 MAC is not configured. Kconfig key: PRESSURE_SENSOR1_BLE_ADDR");
	} else {
		s_links[0].addr_configured = true;
		s_links[0].sensor_index = 0U;
		ESP_LOGI(TAG,
				 "Sensor[0] MAC=%02x:%02x:%02x:%02x:%02x:%02x",
				 s_links[0].addr[0],
				 s_links[0].addr[1],
				 s_links[0].addr[2],
				 s_links[0].addr[3],
				 s_links[0].addr[4],
				 s_links[0].addr[5]);
		init_default_desired_config(&s_links[0]);
	}

	if (!parse_mac_str(CONFIG_PRESSURE_SENSOR2_BLE_ADDR, s_links[1].addr)) {
		ESP_LOGW(TAG, "Sensor2 MAC is not configured. Kconfig key: PRESSURE_SENSOR2_BLE_ADDR");
	} else {
		s_links[1].addr_configured = true;
		s_links[1].sensor_index = 1U;
		ESP_LOGI(TAG,
				 "Sensor[1] MAC=%02x:%02x:%02x:%02x:%02x:%02x",
				 s_links[1].addr[0],
				 s_links[1].addr[1],
				 s_links[1].addr[2],
				 s_links[1].addr[3],
				 s_links[1].addr[4],
				 s_links[1].addr[5]);
		init_default_desired_config(&s_links[1]);
	}

	if (!s_links[0].addr_configured && !s_links[1].addr_configured) {
		ESP_LOGE(TAG, "No pressure sensor MAC configured.");
		return ESP_ERR_INVALID_ARG;
	}

	ESP_LOGI(TAG, "BLE service UUID: df471523-9da9-47cb-abf2-44ba6959979b");
	ESP_LOGI(TAG, "BLE pressure data UUID: df471524-9da9-47cb-abf2-44ba6959979b");
	ESP_LOGI(TAG, "BLE sample period UUID: df471525-9da9-47cb-abf2-44ba6959979b");
	ESP_LOGI(TAG, "BLE report period UUID: df471527-9da9-47cb-abf2-44ba6959979b");
	ESP_LOGI(TAG, "BLE alarm settings UUID: df471528-9da9-47cb-abf2-44ba6959979b");
	ESP_LOGI(TAG, "BLE work mode UUID: df471529-9da9-47cb-abf2-44ba6959979b");

	err = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
	if (err != ESP_OK) {
		ESP_LOGW(TAG, "Classic BT mem release failed: %s", esp_err_to_name(err));
	}

	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
	err = esp_bt_controller_init(&bt_cfg);
	if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
		ESP_LOGE(TAG, "BT controller init failed: %s", esp_err_to_name(err));
		return err;
	}

	err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
	if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
		ESP_LOGE(TAG, "BT controller enable failed: %s", esp_err_to_name(err));
		return err;
	}

	err = esp_bluedroid_init();
	if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
		ESP_LOGE(TAG, "Bluedroid init failed: %s", esp_err_to_name(err));
		return err;
	}

	err = esp_bluedroid_enable();
	if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
		ESP_LOGE(TAG, "Bluedroid enable failed: %s", esp_err_to_name(err));
		return err;
	}

	err = esp_ble_gap_register_callback(gap_cb);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Register GAP callback failed: %s", esp_err_to_name(err));
		return err;
	}

	err = esp_ble_gattc_register_callback(gattc_cb);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Register GATTC callback failed: %s", esp_err_to_name(err));
		return err;
	}

	err = esp_ble_gattc_app_register(0);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Register GATTC app failed: %s", esp_err_to_name(err));
		return err;
	}

	err = esp_ble_gap_set_scan_params(&s_scan_params);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Set scan params failed: %s", esp_err_to_name(err));
		return err;
	}

	if (s_worker_task == NULL) {
		BaseType_t ok = xTaskCreatePinnedToCore(poll_task,
												"pressure_poll",
												4096,
												NULL,
												5,
												&s_worker_task,
												1);
			if (ok != pdPASS) {
				ESP_LOGE(TAG, "Create pressure poll task failed");
				return ESP_FAIL;
			}
		}

	return ESP_OK;
}

/* 注册压力数据回调。 */
esp_err_t PressureSensor_RegisterDataCallback(PressureSensorDataCallback_t callback, void *user_ctx)
{
	if (s_lock == NULL) 
	{
		return ESP_ERR_INVALID_STATE;
	}

	if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(50)) != pdTRUE) 
	{
		return ESP_ERR_TIMEOUT;
	}

	s_data_listener.callback = callback;
	s_data_listener.user_ctx = user_ctx;
	xSemaphoreGive(s_lock);
	return ESP_OK;
}

/* 获取单路压力传感器状态。 */
esp_err_t PressureSensor_GetState(uint8_t sensor_index, PressureSensorState_t *out_state)
{
	if (sensor_index >= PRESSURE_SENSOR_COUNT || out_state == NULL) {
		return ESP_ERR_INVALID_ARG;
	}
	if (s_lock == NULL) {
		return ESP_ERR_INVALID_STATE;
	}

	if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(50)) != pdTRUE) {
		return ESP_ERR_TIMEOUT;
	}
	*out_state = s_links[sensor_index].state;
	xSemaphoreGive(s_lock);
	return ESP_OK;
}

/* 读取单路最新压力数据。 */
esp_err_t pressure_sensor_read(uint8_t sensor_index, PressureSensorData_t *out_data)
{
	PressureSensorState_t state = {0};
	esp_err_t err;

	if (out_data == NULL || sensor_index >= PRESSURE_SENSOR_COUNT) {
		return ESP_ERR_INVALID_ARG;
	}

	err = PressureSensor_GetState(sensor_index, &state);
	if (err != ESP_OK) {
		return err;
	}

	if (!state.connected || !state.data_valid) {
		return ESP_ERR_INVALID_STATE;
	}

	*out_data = state.pressure;
	return ESP_OK;
}

/* 获取全部压力传感器状态。 */
esp_err_t PressureSensor_GetAllState(PressureSensorState_t out_state[PRESSURE_SENSOR_COUNT])
{
	if (out_state == NULL) {
		return ESP_ERR_INVALID_ARG;
	}
	if (s_lock == NULL) {
		return ESP_ERR_INVALID_STATE;
	}

	if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(50)) != pdTRUE) {
		return ESP_ERR_TIMEOUT;
	}
	for (int i = 0; i < PRESSURE_SENSOR_COUNT; i++) {
		out_state[i] = s_links[i].state;
	}
	xSemaphoreGive(s_lock);
	return ESP_OK;
}

/* 设置工作模式。 */
esp_err_t PressureSensor_SetWorkMode(uint8_t sensor_index, PressureSensorWorkMode_t mode)
{
	PressureSensorConfigRequest_t request = {0};

	if (sensor_index >= PRESSURE_SENSOR_COUNT) {
		return ESP_ERR_INVALID_ARG;
	}

	request.set_work_mode = true;
	request.work_mode = mode;
	return PressureSensor_ApplyConfig(sensor_index, &request);
}

/* 设置采集周期。 */
esp_err_t PressureSensor_SetSamplePeriodMs(uint8_t sensor_index, uint32_t period_ms)
{
	PressureSensorConfigRequest_t request = {0};

	if (sensor_index >= PRESSURE_SENSOR_COUNT) {
		return ESP_ERR_INVALID_ARG;
	}

	request.set_sample_period_ms = true;
	request.sample_period_ms = period_ms;
	return PressureSensor_ApplyConfig(sensor_index, &request);
}

/* 设置上报周期。 */
esp_err_t PressureSensor_SetReportPeriodMs(uint8_t sensor_index, uint32_t period_ms)
{
	PressureSensorConfigRequest_t request = {0};

	if (sensor_index >= PRESSURE_SENSOR_COUNT) {
		return ESP_ERR_INVALID_ARG;
	}

	request.set_report_period_ms = true;
	request.report_period_ms = period_ms;
	return PressureSensor_ApplyConfig(sensor_index, &request);
}

/* 设置报警参数。 */
esp_err_t PressureSensor_SetAlarmSettings(uint8_t sensor_index, const PressureSensorAlarmConfig_t *alarm)
{
	PressureSensorConfigRequest_t request = {0};

	if (sensor_index >= PRESSURE_SENSOR_COUNT || alarm == NULL) {
		return ESP_ERR_INVALID_ARG;
	}

	request.set_alarm = true;
	request.alarm = *alarm;
	return PressureSensor_ApplyConfig(sensor_index, &request);
}

/* 批量应用配置请求。 */
esp_err_t PressureSensor_ApplyConfig(uint8_t sensor_index, const PressureSensorConfigRequest_t *config)
{
	if (sensor_index >= PRESSURE_SENSOR_COUNT || config == NULL) {
		return ESP_ERR_INVALID_ARG;
	}
	if (s_lock == NULL) {
		return ESP_ERR_INVALID_STATE;
	}

	if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(50)) != pdTRUE) {
		return ESP_ERR_TIMEOUT;
	}

	esp_err_t err = apply_config_request(&s_links[sensor_index], config);
	xSemaphoreGive(s_lock);
	return err;
}
