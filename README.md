# ESP32LabSensor

基于 ESP-IDF 的实验室多传感器采集系统，运行于 ESP32-S3。项目集成水质、光照、压力传感器采集，支持 LCD 本地显示、按键交互，以及 MQTT 云端上报。

## 1. 项目概览

- 平台：ESP32-S3 + ESP-IDF
- 通信：Wi-Fi、MQTT
- 显示与交互：LVGL + LCD + 按键
- 数据对象：水质、光照、压力（双通道）
- 核心目标：本地可视化 + 云端实时上报

## 2. 程序架构

### 2.1 总体架构

系统采用分层设计：

- 硬件驱动层：各类传感器、LCD、LED、按键驱动
- 服务层：Wi-Fi 连接、MQTT 通信、SNTP 对时、JSON 打包
- 业务层：任务调度、消息队列、数据上报与 UI 更新

### 2.2 任务架构（FreeRTOS）

当前由 `components/App_Tasks/App_Tasks.c` 统一创建以下核心任务：

- `WaterQuality_Sensor_Task`：周期读取水质传感器（15 秒）
- `LightSensor_Task`：周期读取光照传感器（20 秒）
- `PressureSensor_Task`：周期读取 2 路压力传感器（10 秒）
- `LCD_Task`：消费 UI 队列，刷新 UI 与 LVGL 定时处理
- `Key_Task`：按键扫描与 UI 事件注入
- `Upload_Data_Task`：消费 MQTT 队列，JSON 打包并发布消息

### 2.3 队列与消息机制

系统使用两个消息队列进行解耦：

- `Ui_Data_Queue`：传感器任务 -> LCD/UI 任务
- `Mqtt_Data_Queue`：传感器任务 -> MQTT 上传任务

消息结构为 `SensorMessage_t`，根据 `type` 区分：

- `SENSOR_MSG_WATER`
- `SENSOR_MSG_LIGHT`
- `SENSOR_MSG_PRESSURE1`
- `SENSOR_MSG_PRESSURE2`

### 2.4 数据流

1. 启动阶段完成 Wi-Fi、MQTT、SNTP、传感器与 LCD 初始化。
2. 各传感器任务按周期采样，并附加时间戳。
3. 采样结果同时进入 UI 队列和 MQTT 队列。
4. LCD 任务汇总最新数据并更新界面。
5. 上传任务在 MQTT 已连接时将数据打包为 JSON 并发布到对应主题。

## 3. 目录说明

- `main/`：应用入口与系统初始化（`app_main.c`）
- `components/`：各业务组件与驱动模块
  - `App_Tasks/`：任务调度与队列分发（`App_Tasks.c`）
  - `Sensor/`、`PressureSensor/`：传感器采集
  - `Wifi_Connect/`：Wi-Fi 连接管理
  - `Mqtt_Connect/`：MQTT 通信
  - `Json_Utils/`：JSON 打包
  - `LCD_Driver/`：LCD 驱动
  - `LED/`、`KEY/`：基础外设
  - `Timestamp/`：时间同步与时间戳
- `managed_components/`：托管依赖组件（如 LVGL、cJSON 等）
- `partitions.csv`：分区表配置
- `sdkconfig`：项目配置

## 4. MQTT 主题约定

默认上报主题如下（见 `components/App_Tasks/App_Tasks.c`）：

- `sensors/water`
- `sensors/light`
- `sensors/pressure1`
- `sensors/pressure2`

## 5. 版本更新记录

### [v0.1.1] -2026-03-10

- 新增：`App_Tasks` 组件，集中管理 FreeRTOS 任务创建、队列消费与数据分发
- 优化：`main/app_main.c` 精简为系统初始化与启动入口，不再承载任务实现细节
- 修复：待补充

### [v0.1.0] - 2026-03-10

- 初始化项目 README
- 补充程序架构、任务模型与数据流说明
- 预留版本更新记录模板

## 6. 后续维护建议

- 新增传感器时：优先复用 `SensorMessage_t` + 队列分发模式
- 调整采样周期时：同步评估队列长度与 MQTT 上报频率
- 若 UI 刷新卡顿：优先检查 `LCD_Task` 与 `lv_timer_handler()` 调度周期
- 若上报异常：优先检查 Wi-Fi 连接状态、MQTT 连接状态、JSON 打包结果
