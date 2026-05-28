#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"

#include "Sensor.h"
#include "PressureSensor.h"
#include "App_Data.h"
#include "Mqtt_Connect.h"
#include "Wifi_Connect.h"
#include "Json_Utils.h"
#include "KEY.h"
#include "Timestamp.h"

#include "App_Tasks.h"
#include "LCD_Driver.h"
#include "Usb_Storage.h"

#define MQTT_WATER_DATA_PUBLISH_TOPIC "sensors/water"
#define MQTT_LIGHT_DATA_PUBLISH_TOPIC "sensors/light"
#define MQTT_PRESSURE1_DATA_PUBLISH_TOPIC "sensors/pressure1"
#define MQTT_PRESSURE2_DATA_PUBLISH_TOPIC "sensors/pressure2"

#define NORMAL_TASK_STACK_SIZE 3072
#define KEY_TASK_STACK_SIZE 2048

#define SENSOR_TASK_PRIORITY 5
#define UPLOAD_TASK_PRIORITY 5
#define KEY_TASK_PRIORITY 6
#define LCD_TASK_PRIORITY 5

#define SENSOR_QUEUE_LENGTH 16
#define UI_EVENT_QUEUE_LENGTH 8

static const char *TAG = "Lab_Sensor";

/* Calibration command types */
typedef enum {
    CALIB_EC_1413 = 0,
    CALIB_EC_1288,
    CALIB_EC_1113,
    CALIB_PH_401,
    CALIB_PH_686,
    CALIB_PH_918,
    CALIB_ORP_256,
} CalibCmd_t;

/* UI event types */
typedef enum {
    UI_EVENT_CALIBRATION,
    UI_EVENT_SCREEN_SWITCH,
} UiEventType_t;

/* Unified UI event */
typedef struct {
    UiEventType_t type;
    union {
        CalibCmd_t calib_cmd;
    };
} UiEvent_t;

static TaskHandle_t WaterQuality_Sensor_Task_Handle = NULL;
static TaskHandle_t Light_Sensor_Task_Handle = NULL;
static TaskHandle_t Upload_Data_Task_Handle = NULL;
static TaskHandle_t Key_Task_Handle = NULL;
static TaskHandle_t LCD_Task_Handle = NULL;

static QueueHandle_t Ui_Data_Queue = NULL;
static QueueHandle_t Mqtt_Data_Queue = NULL;
static QueueHandle_t Ui_Event_Queue = NULL;
static QueueHandle_t s_pressure_queues[2] = {NULL, NULL};
static SemaphoreHandle_t s_water_modbus_mutex = NULL;
static volatile bool s_calib_running = false;

typedef struct {
    char name[16];
    volatile bool active;
} CalibCountdownCtx_t;

static void Calib_Countdown_Task(void *pvParameters)
{
    CalibCountdownCtx_t *ctx = (CalibCountdownCtx_t *)pvParameters;
    const int total_sec = 40;

    for (int remaining = total_sec; remaining >= 0; remaining--) {
        if (ctx == NULL || !ctx->active) {
            break;
        }

        lcd_update_calibration_status(ctx->name, remaining, total_sec, "Stabilizing sample");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (ctx != NULL && ctx->active) {
        lcd_update_calibration_status(ctx->name, 0, total_sec, "Finishing calibration");
    }

    free(ctx);
    vTaskDelete(NULL);
}

static void PressureSensor_Data_Callback(uint8_t sensor_index, const PressureSensorData_t *data, void *user_ctx)
{
    QueueHandle_t *queues = (QueueHandle_t *)user_ctx;
    SensorMessage_t msg = {0};

    if (queues == NULL || data == NULL) {
        return;
    }

    msg.type = (sensor_index == 0U) ? SENSOR_MSG_PRESSURE1 : SENSOR_MSG_PRESSURE2;
    msg.data.pressure = *data;
    msg.data.pressure.timestamp = SNTP_Get_Timestamp();
    Usb_Storage_LogPressure(sensor_index, &msg.data.pressure);

    if (queues[0] != NULL) {
        xQueueSend(queues[0], &msg, 0);
    }
    if (queues[1] != NULL) {
        xQueueSend(queues[1], &msg, 0);
    }
}

static void WaterQuality_Sensor_Task(void *pvParameters)
{
    WaterQualityData_t Sensor_data = {0};
    SensorMessage_t msg = {0};
    while (1)
    {
        if (s_calib_running) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        ESP_LOGI(TAG, "开始读取传感器数据");
        esp_err_t ret = ESP_ERR_INVALID_STATE;
        if (s_water_modbus_mutex != NULL && xSemaphoreTake(s_water_modbus_mutex, portMAX_DELAY) == pdTRUE) {
            ret = water_sensor_read_all(&Sensor_data);
            xSemaphoreGive(s_water_modbus_mutex);
        }
        Sensor_data.timestamp = SNTP_Get_Timestamp();
        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "传感器数据读取成功");
            ESP_LOGI(TAG, "温度: %.2f C", Sensor_data.temp);
            ESP_LOGI(TAG, "PH值: %.2f", Sensor_data.ph);
            ESP_LOGI(TAG, "TDS值: %lu ppm", Sensor_data.tds);
            ESP_LOGI(TAG, "ORP值: %d mV", Sensor_data.orp);
            ESP_LOGI(TAG, "SALT: %.2f %%", Sensor_data.salt);
            ESP_LOGI(TAG, "SALT_PPT: %.2f ppt", Sensor_data.saltppt);
            ESP_LOGI(TAG, "SG: %.3f sg", Sensor_data.sg);
            ESP_LOGI(TAG, "电导率: %lu uS/cm", Sensor_data.ec);
            ESP_LOGI(TAG, "余氯: %.2f mg/L", Sensor_data.cl);
            msg.type = SENSOR_MSG_WATER;
            msg.data.water = Sensor_data;
            Usb_Storage_LogWater(&Sensor_data);
            xQueueSend(Ui_Data_Queue, &msg, pdMS_TO_TICKS(20));
            xQueueSend(Mqtt_Data_Queue, &msg, pdMS_TO_TICKS(20));
        }
        else
        {
            ESP_LOGE(TAG, "传感器数据读取失败，错误码：%d", ret);
        }

        vTaskDelay(pdMS_TO_TICKS(15000));
    }
}

static void LightSensor_Task(void *pvParameters)
{
    LightSensorData_t Sensor_data = {0};
    SensorMessage_t msg = {0};
    while (1)
    {
        ESP_LOGI(TAG, "开始读取光强传感器数据");
        esp_err_t ret = light_sensor_read_all(&Sensor_data);
        Sensor_data.timestamp = SNTP_Get_Timestamp();
        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "光强传感器数据读取成功");
            ESP_LOGI(TAG, "Light1: %.1f Lux", Sensor_data.lux_1);
            ESP_LOGI(TAG, "Light2: %.1f Lux", Sensor_data.lux_2);
            ESP_LOGI(TAG, "Light3: %.1f Lux", Sensor_data.lux_3);
            ESP_LOGI(TAG, "Light4: %.1f Lux", Sensor_data.lux_4);
            msg.type = SENSOR_MSG_LIGHT;
            msg.data.light = Sensor_data;
            Usb_Storage_LogLight(&Sensor_data);
            xQueueSend(Ui_Data_Queue, &msg, pdMS_TO_TICKS(20));
            xQueueSend(Mqtt_Data_Queue, &msg, pdMS_TO_TICKS(20));
        }
        else
        {
            ESP_LOGE(TAG, "光强传感器数据读取失败，错误码：%d", ret);
        }

        vTaskDelay(pdMS_TO_TICKS(20000));
    }
}

static void Key_Task(void *pvParameters)
{
    KEY_x Keynum = 0;
    int ec_idx = 0;   /* cycles: 0=1413, 1=1288, 2=1113 */
    int ph_idx = 0;   /* cycles: 0=4.01, 1=6.86, 2=9.18 */
    static const CalibCmd_t ec_cmds[] = { CALIB_EC_1413, CALIB_EC_1288, CALIB_EC_1113 };
    static const CalibCmd_t ph_cmds[] = { CALIB_PH_401, CALIB_PH_686, CALIB_PH_918 };
    static const char *ec_names[] = { "EC 1413", "EC 1288", "EC 1113" };
    static const char *ph_names[] = { "PH 4.01", "PH 6.86", "PH 9.18" };

    while (1)
    {
        Keynum = KEY_GetNum();
        if (Keynum == KEY1)
        {
            UiEvent_t evt = { .type = UI_EVENT_CALIBRATION, .calib_cmd = ec_cmds[ec_idx] };
            ESP_LOGI(TAG, "KEY1: 触发 %s 校准", ec_names[ec_idx]);
            ec_idx = (ec_idx + 1) % 3;
            xQueueSend(Ui_Event_Queue, &evt, 0);
        }
        else if (Keynum == KEY2)
        {
            UiEvent_t evt = { .type = UI_EVENT_CALIBRATION, .calib_cmd = ph_cmds[ph_idx] };
            ESP_LOGI(TAG, "KEY2: 触发 %s 校准", ph_names[ph_idx]);
            ph_idx = (ph_idx + 1) % 3;
            xQueueSend(Ui_Event_Queue, &evt, 0);
        }
        else if (Keynum == KEY3)
        {
            UiEvent_t evt = { .type = UI_EVENT_CALIBRATION, .calib_cmd = CALIB_ORP_256 };
            ESP_LOGI(TAG, "KEY3: 触发 ORP 256 校准");
            xQueueSend(Ui_Event_Queue, &evt, 0);
        }
        else if (Keynum == KEY4)
        {
            UiEvent_t evt = { .type = UI_EVENT_SCREEN_SWITCH };
            ESP_LOGI(TAG, "KEY4: 切换屏幕");
            xQueueSend(Ui_Event_Queue, &evt, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/* Calibration task: runs in background, blocks for 40s, then returns to main screen */
static void Calib_Task(void *pvParameters)
{
    CalibCmd_t cmd = *(CalibCmd_t *)pvParameters;
    free(pvParameters);

    static const char *names[] = {
        "EC 1413", "EC 1288", "EC 1113",
        "PH 4.01", "PH 6.86", "PH 9.18",
        "ORP 256"
    };

    typedef esp_err_t (*calib_func_t)(void);
    static const calib_func_t funcs[] = {
        water_sensor_calib_ec_1413,
        water_sensor_calib_ec_1288,
        water_sensor_calib_ec_1113,
        water_sensor_calib_ph_4_01,
        water_sensor_calib_ph_6_86,
        water_sensor_calib_ph_9_18,
        water_sensor_calib_orp_256,
    };

    ESP_LOGI(TAG, "校准任务启动: %s", names[cmd]);
    lcd_show_calibration(names[cmd]);

    CalibCountdownCtx_t *countdown_ctx = calloc(1, sizeof(CalibCountdownCtx_t));
    if (countdown_ctx != NULL) {
        strncpy(countdown_ctx->name, names[cmd], sizeof(countdown_ctx->name) - 1);
        countdown_ctx->active = true;
        lcd_update_calibration_status(countdown_ctx->name, 40, 40, "Stabilizing sample");
        if (xTaskCreatePinnedToCore(Calib_Countdown_Task, "Calib_Countdown", 3072,
                                    countdown_ctx, LCD_TASK_PRIORITY - 1, NULL, 0) != pdPASS) {
            free(countdown_ctx);
            countdown_ctx = NULL;
        }
    }

    esp_err_t ret = ESP_ERR_INVALID_STATE;
    if (s_water_modbus_mutex != NULL && xSemaphoreTake(s_water_modbus_mutex, portMAX_DELAY) == pdTRUE) {
        ret = funcs[cmd]();
        xSemaphoreGive(s_water_modbus_mutex);
    }
    if (countdown_ctx != NULL) {
        countdown_ctx->active = false;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "校准 %s 失败: %s", names[cmd], esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "校准 %s 完成", names[cmd]);
    }

    if (ret != ESP_OK) {
        lcd_update_calibration_status(names[cmd], 0, 40, "校准失败");
    } else {
        lcd_update_calibration_status(names[cmd], 0, 40, "校准完成");
    }
    vTaskDelay(pdMS_TO_TICKS(800));
    s_calib_running = false;
    lcd_switch_to_main();
    vTaskDelete(NULL);
}

static void LCD_Task(void *pvParameters)
{
    SensorMessage_t msg = {0};
    WaterQualityData_t water_data = {0};
    LightSensorData_t light_data = {0};
    PressureSensorData_t pressure1_data = {0};
    PressureSensorData_t pressure2_data = {0};
    bool has_water_data = false;
    bool has_light_data = false;
    bool has_pressure1_data = false;
    bool has_pressure2_data = false;

    bool showing_main = true;
    TickType_t last_screen_switch_tick = xTaskGetTickCount();
    const TickType_t screen_switch_interval = pdMS_TO_TICKS(30000);

    if (!LCD_Driver_IsReady()) {
        ESP_LOGW(TAG, "LCD 驱动未准备好，LCD 任务无法启动");
        LCD_Task_Handle = NULL;
        vTaskDelete(NULL);
    }

    lcd_switch_to_main();

    while (1)
    {
        /* Process UI events (calibration + screen switch) */
        UiEvent_t ui_evt;
        if (!s_calib_running && xQueueReceive(Ui_Event_Queue, &ui_evt, 0) == pdTRUE) {
            if (ui_evt.type == UI_EVENT_CALIBRATION) {
                CalibCmd_t *cmd_copy = malloc(sizeof(CalibCmd_t));
                if (cmd_copy != NULL) {
                    *cmd_copy = ui_evt.calib_cmd;
                    s_calib_running = true;
                    showing_main = true;
                    last_screen_switch_tick = xTaskGetTickCount();
                    xTaskCreatePinnedToCore(Calib_Task, "Calib_Task", 4096, cmd_copy,
                                            SENSOR_TASK_PRIORITY, NULL, 0);
                }
            } else if (ui_evt.type == UI_EVENT_SCREEN_SWITCH) {
                showing_main = !showing_main;
                last_screen_switch_tick = xTaskGetTickCount();
                if (showing_main) {
                    lcd_switch_to_main();
                } else {
                    lcd_switch_to_light();
                }
            }
        }

        /* Process sensor data queue */
        while (xQueueReceive(Ui_Data_Queue, &msg, 0) == pdTRUE)
        {
            if (msg.type == SENSOR_MSG_WATER)
            {
                water_data = msg.data.water;
                has_water_data = true;
            }
            else if (msg.type == SENSOR_MSG_LIGHT)
            {
                light_data = msg.data.light;
                has_light_data = true;
            }
            else if (msg.type == SENSOR_MSG_PRESSURE1)
            {
                pressure1_data = msg.data.pressure;
                has_pressure1_data = true;
            }
            else if (msg.type == SENSOR_MSG_PRESSURE2)
            {
                pressure2_data = msg.data.pressure;
                has_pressure2_data = true;
            }
        }

        lcd_ui_update_sensor_data(has_water_data ? &water_data : NULL,
                                  has_light_data ? &light_data : NULL,
                                  has_pressure1_data ? &pressure1_data : NULL,
                                  has_pressure2_data ? &pressure2_data : NULL);

        if (!s_calib_running && (xTaskGetTickCount() - last_screen_switch_tick) >= screen_switch_interval) {
            showing_main = !showing_main;
            last_screen_switch_tick = xTaskGetTickCount();
            if (showing_main) {
                lcd_switch_to_main();
            } else {
                lcd_switch_to_light();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void Upload_Data_Task(void *pvParameters)
{
    SensorMessage_t msg = {0};
    char *Json_Data = NULL;
    while (1)
    {
        if (!Wifi_IS_Connected())
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (Mqtt_Is_Connectecd())
        {
            if (xQueueReceive(Mqtt_Data_Queue, &msg, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                if (msg.type == SENSOR_MSG_WATER)
                {
                    WaterQualityData_t water_data = msg.data.water;
                    ESP_LOGI(TAG, "收到新水质数据，准备上报MQTT服务器");
                    Json_Data = JSON_PackSensorData(&water_data);
                    if (Json_Data != NULL)
                    {
                        ESP_LOGI(TAG, "传感器数据JSON打包成功，数据：%s", Json_Data);
                        Mqtt_Publish_Message(MQTT_WATER_DATA_PUBLISH_TOPIC, Json_Data, strlen(Json_Data), 1, 0);
                        free(Json_Data);
                    }
                }
                else if (msg.type == SENSOR_MSG_LIGHT)
                {
                    LightSensorData_t light_data = msg.data.light;
                    ESP_LOGI(TAG, "收到新光照数据，准备上报MQTT服务器");
                    Json_Data = JSON_PackLightData(&light_data);
                    if (Json_Data != NULL)
                    {
                        ESP_LOGI(TAG, "光照数据JSON打包成功，数据：%s", Json_Data);
                        Mqtt_Publish_Message(MQTT_LIGHT_DATA_PUBLISH_TOPIC, Json_Data, strlen(Json_Data), 1, 0);
                        free(Json_Data);
                    }
                }
                else if (msg.type == SENSOR_MSG_PRESSURE1)
                {
                    PressureSensorData_t pressure1_data = msg.data.pressure;
                    ESP_LOGI(TAG, "收到新压力1数据，准备上报MQTT服务器");
                    Json_Data = JSON_PackPressureData(&pressure1_data);
                    if (Json_Data != NULL)
                    {
                        ESP_LOGI(TAG, "压力1数据JSON打包成功，数据：%s", Json_Data);
                        Mqtt_Publish_Message(MQTT_PRESSURE1_DATA_PUBLISH_TOPIC, Json_Data, strlen(Json_Data), 1, 0);
                        free(Json_Data);
                    }
                }
                else if (msg.type == SENSOR_MSG_PRESSURE2)
                {
                    PressureSensorData_t pressure2_data = msg.data.pressure;
                    ESP_LOGI(TAG, "收到新压力2数据，准备上报MQTT服务器");
                    Json_Data = JSON_PackPressureData(&pressure2_data);
                    if (Json_Data != NULL)
                    {
                        ESP_LOGI(TAG, "压力2数据JSON打包成功，数据：%s", Json_Data);
                        Mqtt_Publish_Message(MQTT_PRESSURE2_DATA_PUBLISH_TOPIC, Json_Data, strlen(Json_Data), 1, 0);
                        free(Json_Data);
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void App_Start_Upload_Task(void)
{
    if (Upload_Data_Task_Handle != NULL) {
        return;
    }

    if (Mqtt_Data_Queue == NULL) {
        ESP_LOGW(TAG, "MQTT数据队列未创建，无法启动上传任务");
        return;
    }

    xTaskCreatePinnedToCore((TaskFunction_t)&Upload_Data_Task,
                            (const char * const)"Upload_Data_Task",
                            (const configSTACK_DEPTH_TYPE)NORMAL_TASK_STACK_SIZE,
                            (void * const)NULL,
                            (UBaseType_t)UPLOAD_TASK_PRIORITY,
                            (TaskHandle_t * const)&Upload_Data_Task_Handle,
                            (const BaseType_t)0);
    if (Upload_Data_Task_Handle != NULL)
    {
        ESP_LOGI(TAG, "上传任务创建成功");
    }
    else
    {
        ESP_LOGE(TAG, "上传任务创建失败");
    }
}

void App_Create_Task(void)
{
    Ui_Data_Queue = xQueueCreate(SENSOR_QUEUE_LENGTH, sizeof(SensorMessage_t));
    Mqtt_Data_Queue = xQueueCreate(SENSOR_QUEUE_LENGTH, sizeof(SensorMessage_t));
    Ui_Event_Queue = xQueueCreate(UI_EVENT_QUEUE_LENGTH, sizeof(UiEvent_t));
    s_water_modbus_mutex = xSemaphoreCreateMutex();

    if (Ui_Data_Queue != NULL && Mqtt_Data_Queue != NULL && Ui_Event_Queue != NULL && s_water_modbus_mutex != NULL)
    {
        ESP_LOGI(TAG, "传感器消息队列创建成功");
    }
    else
    {
        ESP_LOGE(TAG, "传感器消息队列创建失败");
    }

    s_pressure_queues[0] = Ui_Data_Queue;
    s_pressure_queues[1] = Mqtt_Data_Queue;
    KEY_Init();

    if (PressureSensor_RegisterDataCallback(PressureSensor_Data_Callback, s_pressure_queues) == ESP_OK)
    {
        ESP_LOGI(TAG, "压力传感器回调注册成功");
    }
    else
    {
        ESP_LOGE(TAG, "压力传感器回调注册失败");
    }

    xTaskCreatePinnedToCore((TaskFunction_t)&WaterQuality_Sensor_Task,
                            (const char * const)"Sensor_Task",
                            (const configSTACK_DEPTH_TYPE)NORMAL_TASK_STACK_SIZE,
                            (void * const)NULL,
                            (UBaseType_t)SENSOR_TASK_PRIORITY,
                            (TaskHandle_t * const)&WaterQuality_Sensor_Task_Handle,
                            (const BaseType_t)1);
    if (WaterQuality_Sensor_Task_Handle != NULL)
    {
        ESP_LOGI(TAG, "水质传感器任务创建成功");
    }
    else
    {
        ESP_LOGI(TAG, "水质传感器任务创建失败");
    }

    xTaskCreatePinnedToCore((TaskFunction_t)&LightSensor_Task,
                            (const char * const)"Light_Task",
                            (const configSTACK_DEPTH_TYPE)NORMAL_TASK_STACK_SIZE,
                            (void * const)NULL,
                            (UBaseType_t)SENSOR_TASK_PRIORITY,
                            (TaskHandle_t * const)&Light_Sensor_Task_Handle,
                            (const BaseType_t)1);
    if (Light_Sensor_Task_Handle != NULL)
    {
        ESP_LOGI(TAG, "光强传感器任务创建成功");
    }
    else
    {
        ESP_LOGI(TAG, "光强传感器任务创建失败");
    }

    xTaskCreatePinnedToCore((TaskFunction_t)&Key_Task,
                            (const char * const)"Key_Task",
                            (const configSTACK_DEPTH_TYPE)KEY_TASK_STACK_SIZE,
                            (void * const)NULL,
                            (UBaseType_t)KEY_TASK_PRIORITY,
                            (TaskHandle_t * const)&Key_Task_Handle,
                            (const BaseType_t)1);
    if (Key_Task_Handle != NULL)
    {
        ESP_LOGI(TAG, "按键任务创建成功");
    }
    else
    {
        ESP_LOGI(TAG, "按键任务创建失败");
    }

    if (LCD_Driver_IsReady())
    {
        xTaskCreatePinnedToCore((TaskFunction_t)&LCD_Task,
                                (const char * const)"LCD_Task",
                                (const configSTACK_DEPTH_TYPE)NORMAL_TASK_STACK_SIZE,
                                (void * const)NULL,
                                (UBaseType_t)LCD_TASK_PRIORITY,
                                (TaskHandle_t * const)&LCD_Task_Handle,
                                (const BaseType_t)0);
        if (LCD_Task_Handle != NULL)
        {
            ESP_LOGI(TAG, "LCD任务创建成功");
        }
        else
        {
            ESP_LOGI(TAG, "LCD任务创建失败");
        }
    }
    else
    {
        ESP_LOGW(TAG, "LCD未就绪，跳过LCD任务");
    }

    ESP_LOGI(TAG, "上报任务将在网络就绪后启动");
}
