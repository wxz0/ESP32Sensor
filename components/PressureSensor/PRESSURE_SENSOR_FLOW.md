# 压力传感器读取流程文档

## 概述
压力传感器通过 **BLE GATT 客户端** 连接到远程传感器设备，获取实时压力数据。系统支持 **2 路独立传感器**，采用事件驱动 + 队列推送模式，确保数据可靠性。

---

## 1. 初始化流程（Init Phase）

### 1.1 入口函数
```c
// main/app_main.c
void app_main(void)
{
    // ... 其他初始化
    if (PressureSensor_Init() != ESP_OK) {
        ESP_LOGW(TAG, "压力传感器 BLE 初始化失败，请检查蓝牙配置和 MAC 地址");
    }
    // ... 继续启动其他任务
}
```

### 1.2 PressureSensor_Init() 核心流程

| 步骤 | 操作 | 输出日志 | 说明 |
|------|------|---------|------|
| 1 | 创建互斥信号量 `s_lock` | - | 保护 BLE 链接状态和回调 |
| 2 | 解析 Kconfig 配置的 MAC 地址 | `Sensor[0] MAC=xx:xx:xx:xx:xx:xx` | 从 `CONFIG_PRESSURE_SENSOR1/2_BLE_ADDR` 读取 |
| 3 | 初始化传感器链接结构体 | - | 设置默认工作模式为**网关模式** (`GATEWAY`) |
| 4 | 释放经典蓝牙内存 | `Classic BT mem release failed: ...` (warn) | 节省 ESP32 内存 |
| 5 | 初始化蓝牙控制器 | - | 配置为 **BLE 模式** |
| 6 | 启用蓝牙控制器 | - | 状态: `ESP_BT_MODE_BLE` |
| 7 | 初始化 Bluedroid 协议栈 | - | 蓝牙 HOST 层 |
| 8 | 启用 Bluedroid | - | 准备 GAP/GATT 子系统 |
| 9 | 注册 GAP 回调 `gap_cb` | - | 处理扫描结果、连接状态 |
| 10 | 注册 GATTC 回调 `gattc_cb` | - | 处理服务发现、特征读写、通知 |
| 11 | 注册 GATTC App (ID=0) | - | 创建虚拟 GATT 应用 |
| 12 | 设置 BLE 扫描参数 | - | 主动扫描、扫描间隔 0x50、窗口 0x30 |
| 13 | 创建后台轮询任务 | `Create pressure poll task failed` (error) | 优先级 5、核心 1、4KB 栈 |

#### 打印的服务 & 特征 UUID
```log
[INFO] BLE service UUID: df471523-9da9-47cb-abf2-44ba6959979b
[INFO] BLE pressure data UUID: df471524-9da9-47cb-abf2-44ba6959979b
[INFO] BLE sample period UUID: df471525-9da9-47cb-abf2-44ba6959979b
[INFO] BLE report period UUID: df471527-9da9-47cb-abf2-44ba6959979b
[INFO] BLE alarm settings UUID: df471528-9da9-47cb-abf2-44ba6959979b
[INFO] BLE work mode UUID: df471529-9da9-47cb-abf2-44ba6959979b
```

---

## 2. 扫描与连接流程（Scan & Connect Phase）

### 2.1 BLE 扫描启动

**触发条件**：
- `s_ble_ready` = true（Bluedroid 就绪）
- `s_scan_param_ready` = true（扫描参数已设置）
- `s_gattc_if` ≠ ESP_GATT_IF_NONE（GATTC 接口已注册）

**时序**：
1. `esp_ble_gap_set_scan_params()` → 触发 `ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT`
2. GAP 回调 `gap_cb()` → 调用 `try_start_scan()`
3. `esp_ble_gap_start_scanning()` → 启动扫描

**日志输出**：
```log
[WARN] Start scan failed: ...  // 仅当 start_scanning 返回错误时
```

### 2.2 扫描结果处理

**关键事件**：`ESP_GAP_BLE_SCAN_RESULT_EVT`

| 扫描阶段 | 处理 | 日志 |
|---------|------|------|
| `ESP_GAP_SEARCH_INQ_RES_EVT` | 单个设备广告 | `Found sensor[X], addr=xx:xx:xx:xx:xx:xx, addr_type=Y. Opening...` |
| | 如果已配置 MAC 且未连接，发起连接 | - |
| `ESP_GAP_SEARCH_INQ_CMPL_EVT` | 扫描周期完成 | - |
| | 重新启动新一轮扫描 | - |

### 2.3 连接建立

**事件**：`ESP_GATTC_OPEN_EVT`

```
[INFO] Sensor[0] connected, conn_id=XX, mtu=YY
```

| 状态 | 日志 | 动作 |
|------|------|------|
| 连接成功 | `connected, conn_id=...` | 清空特征句柄、搜索服务 |
| 连接失败 | `open failed, status=...` | 标记未连接、重启扫描 |

**内部状态更新**：
```c
link->connected = true;
link->state.connected = true;
link->state.data_valid = false;
link->conn_id = XX;
esp_ble_gattc_search_service(gattc_if, link->conn_id, UUID);
```

---

## 3. 服务与特征发现（Service & Characteristic Discovery）

### 3.1 服务搜索结果

**事件**：`ESP_GATTC_SEARCH_RES_EVT`

```log
[INFO] Sensor[0] service found: [0x0001-0x00FF]
```

存储了服务的起止 handle，用于后续特征查询。

### 3.2 服务发现完成

**事件**：`ESP_GATTC_SEARCH_CMPL_EVT`

```log
[INFO] Sensor[0] service discovery complete, status=0
```

**操作序列**：
1. 调用 `update_char_handles()` 查询所有特征句柄
2. 依次读取 6 个特征：
   - Pressure (会存储 CCCD 描述符)
   - Sample Period
   - Report Period
   - Alarm Settings
   - Work Mode
3. 注册 Pressure 特征的 notify

**特征句柄打印**：
```log
[INFO] Sensor[0] handles: pressure=0x001C cccd=0x001D sample=0x001F report=0x0022 alarm=0x0025 mode=0x0028
```

---

## 4. 数据接收与解析（Data Receive & Parse）

### 4.1 Notify 注册流程

**步骤**：

| 事件 | 操作 | 日志 |
|------|------|------|
| `ESP_GATTC_REG_FOR_NOTIFY_EVT` | 注册成功 | `Sensor[0] enabling notify via CCCD=0x001D` |
| | 写 CCCD 描述符 | - |
| | 写值 = 0x0001（使能通知） | - |

### 4.2 Notify 到达与数据读取

**事件流**：

```
外设 notify → ESP_GATTC_NOTIFY_EVT
   ↓
[DEBUG] Sensor[0] notify received, triggering read
   ↓
esp_ble_gattc_read_char() 主动读特征
   ↓
ESP_GATTC_READ_CHAR_EVT
```

**日志**：
```log
[DEBUG] Sensor[0] notify received, triggering read
[WARN] Read char failed: conn_id=1 handle=0x001C status=3  // 仅失败时
```

### 4.3 特征数据解析

**事件**：`ESP_GATTC_READ_CHAR_EVT`

根据 handle 类型调用对应解析函数：

| Handle | 函数 | 功能 |
|--------|------|------|
| `pressure_char_handle` | `parse_pressure_payload()` | 解析压力值（14 字节），触发回调 |
| `sample_char_handle` | `parse_period_payload(is_sample=true)` | 解析采集周期元数据 |
| `report_char_handle` | `parse_period_payload(is_sample=false)` | 解析上报周期元数据 |
| `alarm_char_handle` | `parse_alarm_payload()` | 解析报警参数 |
| `work_mode_char_handle` | `parse_work_mode_payload()` | 解析工作模式 |

**关键数据结构** (`PressureSensorState_t`)：
```c
typedef struct {
    PressureSensorData_t pressure;           // 实时压力值
    uint8_t alarm_status;                    // 报警状态
    uint32_t sample_period_ms;               // 采集周期（毫秒）
    uint32_t report_period_ms;               // 上报周期（毫秒）
    PressureSensorAlarmConfig_t alarm;       // 报警参数
    PressureSensorWorkMode_t work_mode;      // 工作模式
    bool connected;                          // 连接状态
    bool data_valid;                         // 数据有效标志
    bool config_valid;                       // 配置有效标志
} PressureSensorState_t;
```

---

## 5. 回调与上层应用（Callback & Upper Layer）

### 5.1 数据回调函数

**触发时机**：压力数据成功解析后立即回调

```c
// 在 parse_pressure_payload() 中调用
if (s_data_listener.callback != NULL) {
    s_data_listener.callback(link->sensor_index, &link->state.pressure, s_data_listener.user_ctx);
}
```

### 5.2 App_Tasks 中的回调实现

**注册点**（`App_Tasks.c`）：
```c
void App_Create_Task(void)
{
    // ...
    s_pressure_queues[0] = Ui_Data_Queue;
    s_pressure_queues[1] = Mqtt_Data_Queue;
    
    if (PressureSensor_RegisterDataCallback(PressureSensor_Data_Callback, s_pressure_queues) == ESP_OK) {
        ESP_LOGI(TAG, "压力传感器回调注册成功");
    }
}
```

**回调实现**：
```c
static void PressureSensor_Data_Callback(uint8_t sensor_index, 
                                         const PressureSensorData_t *data, 
                                         void *user_ctx)
{
    QueueHandle_t *queues = (QueueHandle_t *)user_ctx;
    SensorMessage_t msg = {0};

    msg.type = (sensor_index == 0U) ? SENSOR_MSG_PRESSURE1 : SENSOR_MSG_PRESSURE2;
    msg.data.pressure = *data;
    msg.data.pressure.timestamp = SNTP_Get_Timestamp();  // 附加时间戳

    // 送入两个队列：UI 队列 + MQTT 队列
    if (queues[0] != NULL) {
        xQueueSend(queues[0], &msg, 0);  // 不阻塞
    }
    if (queues[1] != NULL) {
        xQueueSend(queues[1], &msg, 0);
    }
}
```

### 5.3 MQTT 上报任务处理

**任务名**：`Upload_Data_Task`

```c
static void Upload_Data_Task(void *pvParameters)
{
    SensorMessage_t msg = {0};
    char *Json_Data = NULL;
    
    while (1) {
        if (Mqtt_Is_Connectecd()) {
            if (xQueueReceive(Mqtt_Data_Queue, &msg, pdMS_TO_TICKS(50)) == pdTRUE) {
                if (msg.type == SENSOR_MSG_PRESSURE1) {
                    PressureSensorData_t pressure1_data = msg.data.pressure;
                    ESP_LOGI(TAG, "收到新压力1数据，准备上报MQTT服务器");
                    
                    Json_Data = JSON_PackPressureData(&pressure1_data);
                    if (Json_Data != NULL) {
                        ESP_LOGI(TAG, "压力1数据JSON打包成功，数据：%s", Json_Data);
                        Mqtt_Publish_Message(MQTT_PRESSURE1_DATA_PUBLISH_TOPIC, 
                                           Json_Data, 
                                           strlen(Json_Data), 
                                           1, 0);
                        free(Json_Data);
                    }
                }
                // 类似处理 SENSOR_MSG_PRESSURE2
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

**发布主题**：
- `sensors/pressure1` - 第一路传感器数据
- `sensors/pressure2` - 第二路传感器数据

---

## 6. 配置下发流程（Config Apply）

### 6.1 轮询任务中的配置应用

**后台任务**：`poll_task`（优先级 5、核心 1）

```c
static void poll_task(void *arg)
{
    while (1) {
        if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(50)) == pdTRUE) {
            for (int i = 0; i < PRESSURE_SENSOR_COUNT; i++) {
                pressure_sensor_link_t *link = &s_links[i];
                
                if (!link->connected || link->conn_id == 0U) {
                    continue;
                }
                
                // 执行挂起的配置下发
                apply_pending_config(link);
                
                // 读取配置参数（仅当 config_valid=false 时）
                if (link->sample_char_handle != 0U && !link->state.config_valid) {
                    esp_ble_gattc_read_char(...);
                }
                // 类似读取 report、alarm、work_mode
            }
            xSemaphoreGive(s_lock);
        }
        vTaskDelay(pdMS_TO_TICKS(CONFIG_PRESSURE_SENSOR_BLE_POLL_INTERVAL_MS));  // 默认 1000ms
    }
}
```

### 6.2 配置请求入队

**API 调用**（来自上层应用）：

```c
// 设置工作模式
PressureSensor_SetWorkMode(0, PRESSURE_SENSOR_WORK_MODE_GATEWAY);
// 输出日志：[INFO] Sensor[0] queue work_mode=255

// 设置采集周期
PressureSensor_SetSamplePeriodMs(0, 5000);
// 输出日志：[INFO] Sensor[0] queue sample_period_ms=5000

// 设置上报周期
PressureSensor_SetReportPeriodMs(0, 10000);
// 输出日志：[INFO] Sensor[0] queue report_period_ms=10000

// 设置报警参数
PressureSensorAlarmConfig_t alarm = {
    .low_alarm_pa = 50000.0f,
    .high_alarm_pa = 150000.0f,
    .oscillation_alarm_pa = 5000.0f
};
PressureSensor_SetAlarmSettings(0, &alarm);
// 输出日志：[INFO] Sensor[0] queue alarm high=150000.000000 low=50000.000000
```

### 6.3 配置下发执行

**步骤**（sequential）：

| 优先级 | 配置项 | 执行函数 | 日志（成功） | 日志（失败） |
|--------|--------|---------|-------------|-------------|
| 1 | Work Mode | `write_work_mode()` | `write work_mode=...` | `write work_mode failed: ...` |
| 2 | Sample Period | `write_sample_period()` | `write sample_period_ms=...` | `write sample_period failed: ...` |
| 3 | Report Period | `write_report_period()` | `write report_period_ms=...` | `write report_period failed: ...` |
| 4 | Alarm | `write_alarm_settings()` | `write alarm high=... low=...` | `write alarm failed: ...` |

> **注意**：每次轮询只执行 **一个** 配置项，确保不会因 BLE 繁忙而丢弃请求。

---

## 7. 断连与重连流程（Disconnect & Reconnect）

### 7.1 断连事件

**事件**：`ESP_GATTC_DISCONNECT_EVT`

```log
[WARN] Disconnected: addr=xx:xx:xx:xx:xx:xx reason=0x13
```

**内部清理**：
```c
link->connected = false;
link->data_valid = false;
link->conn_id = 0;
// 清空所有特征句柄
link->pressure_char_handle = 0;
link->sample_char_handle = 0;
// ...
link->state.connected = false;
```

### 7.2 重连机制

- 断连后立即调用 `try_start_scan()`
- BLE 扫描线程重新发现设备
- 发现已配置 MAC 时再次发起连接

---

## 8. API 总结表

### 初始化 & 注册

| API | 功能 | 返回值 |
|-----|------|--------|
| `PressureSensor_Init()` | 初始化 BLE 传感器模块 | `ESP_OK` / `ESP_ERR_*` |
| `PressureSensor_RegisterDataCallback()` | 注册数据回调 | `ESP_OK` / `ESP_ERR_*` |

### 数据读取

| API | 功能 | 返回值 |
|-----|------|--------|
| `PressureSensor_GetState(idx, *state)` | 获取单路传感器完整状态 | `ESP_OK` / `ESP_ERR_*` |
| `PressureSensor_GetAllState(*state[2])` | 获取两路传感器状态 | `ESP_OK` / `ESP_ERR_*` |
| `pressure_sensor_read(idx, *data)` | 读取单路最新压力值 | `ESP_OK` / `ESP_ERR_*` |

### 配置下发

| API | 功能 | 返回值 |
|-----|------|--------|
| `PressureSensor_SetWorkMode(idx, mode)` | 设置工作模式 | `ESP_OK` / `ESP_ERR_*` |
| `PressureSensor_SetSamplePeriodMs(idx, ms)` | 设置采集周期 | `ESP_OK` / `ESP_ERR_*` |
| `PressureSensor_SetReportPeriodMs(idx, ms)` | 设置上报周期 | `ESP_OK` / `ESP_ERR_*` |
| `PressureSensor_SetAlarmSettings(idx, *alarm)` | 设置报警参数 | `ESP_OK` / `ESP_ERR_*` |
| `PressureSensor_ApplyConfig(idx, *config)` | 批量应用配置 | `ESP_OK` / `ESP_ERR_*` |

---

## 9. 数据流图示

```
┌─────────────────────────────────────────────────────────────┐
│              压力传感器完整数据流                              │
└─────────────────────────────────────────────────────────────┘

   初始化
   app_main()
      │
      └─→ PressureSensor_Init()
            ├─ 初始化 BLE 控制器/Bluedroid
            ├─ 注册 GAP/GATTC 回调
            ├─ 启动 BLE 扫描
            └─ 启动后台轮询任务
            
   扫描与连接
      │
      ├─→ ESP_GAP_BLE_SCAN_RESULT_EVT (GAP 回调)
      │     └─ 发现设备 MAC → esp_ble_gattc_open()
      │
      └─→ ESP_GATTC_OPEN_EVT (GATTC 回调)
            └─ 连接建立 → esp_ble_gattc_search_service()
            
   服务与特征发现
      │
      ├─→ ESP_GATTC_SEARCH_RES_EVT
      │     └─ [0x0001-0x00FF] 服务范围
      │
      └─→ ESP_GATTC_SEARCH_CMPL_EVT
            └─ update_char_handles() → esp_ble_gattc_register_for_notify()
            
   数据接收
      │
      ├─→ ESP_GATTC_REG_FOR_NOTIFY_EVT
      │     └─ 写 CCCD (0x0001) → 使能 notify
      │
      └─→ ESP_GATTC_NOTIFY_EVT (来自外设)
            └─ esp_ble_gattc_read_char()
            
   数据解析与回调
      │
      ├─→ ESP_GATTC_READ_CHAR_EVT
      │     ├─ parse_pressure_payload()
      │     └─ PressureSensor_Data_Callback()
      │
      └─→ xQueueSend(Mqtt_Data_Queue)
            
   MQTT 上报
      │
      └─→ Upload_Data_Task
            └─ JSON_PackPressureData()
                  └─ Mqtt_Publish_Message("sensors/pressure1", ...)
```

---

## 10. 调试日志等级参考

### 快速定位问题的关键日志

| 问题 | 查看日志 | 期望结果 |
|------|---------|---------|
| 无法连接设备 | `Found sensor[X], addr=...` | 应出现扫描发现日志 |
| 连接中断 | `Disconnected: ... reason=...` | 检查断连原因码 |
| 无法获取特征 | `Sensor[X] handles: pressure=0x...` | CCCD 不应为 0 |
| notify 不工作 | `enabling notify via CCCD=...` | 应跟随注册成功日志 |
| 配置下发失败 | `write work_mode failed: ...` | 检查错误代码 |
| 栈溢出（Key_Task 相关） | `Key_Task stack high watermark: ...` | 应 > 100 words |

### 日志等级设置

```c
// app_main.c
esp_log_level_set("*", ESP_LOG_INFO);              // 全局 INFO 级
esp_log_level_set("PressureSensor", ESP_LOG_DEBUG); // 细粒度调试
```

---

## 11. 常见故障排查

### 11.1 `Press Sensor Init failed`
- **原因**：MAC 地址未配置或格式错误
- **解决**：检查 Kconfig `CONFIG_PRESSURE_SENSOR1/2_BLE_ADDR`（格式：`xx:xx:xx:xx:xx:xx`）

### 11.2 连接后立即断开
- **原因**：MTU 不匹配或服务发现超时
- **排查**：查看日志 `connected, conn_id=..., mtu=...` 中 MTU 值

### 11.3 notify 未触发
- **原因**：CCCD 写入失败或外设未使能通知
- **排查**：`enabling notify via CCCD=...` 是否出现；查看 `Read char failed` 日志

### 11.4 数据始终无效
- **原因**：`data_valid` 标志未置位
- **排查**：检查 `parse_pressure_payload()` 是否被调用；数据长度是否 >= 14 字节

---

## 总结

**完整的数据读取链路**：

```
Init → Scan → Connect → ServiceDiscovery → RegisterNotify 
   ↓    ↓        ↓            ↓               ↓
  BLE  GAP     GATTC        GATTC          GATTC
 初始化 扫描    打开连接      查询特征        注册通知
                              ↓
                         ApplyConfig (poll_task 后续)
                              ↓
                          ReceiveNotify
                              ↓
                          ReadCharacteristic
                              ↓
                          ParsePressureData
                              ↓
                       InvokeDataCallback
                              ↓
                         QueueSend (→ Upload_Data_Task)
                              ↓
                        Mqtt_Publish_Message
```

每个环节都有对应的日志输出，便于快速定位问题所在。
