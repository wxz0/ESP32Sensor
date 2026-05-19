# MQTT 连接与上报流程文档

## 概述
MQTT 模块通过 `Mqtt_Connect` 组件创建客户端并维护连接状态，上层任务通过队列消费传感器消息并发布到 Broker。连接层和业务上报层解耦，便于自动重连后继续发送数据。

---

## 1. 初始化流程（Init Phase）

### 1.1 入口函数
```c
// main/app_main.c
ConnectToMqtt(MQTT_BROKER_URI, MQTT_USERNAME, MQTT_PASSWORD);
```

### 1.2 `ConnectToMqtt()` 核心流程

| 步骤 | 操作 | 输出日志 | 说明 |
|------|------|---------|------|
| 1 | 构造 `esp_mqtt_client_config_t` | - | 设置 Broker、认证和重连参数 |
| 2 | `esp_mqtt_client_init(&mqtt_cfg)` | `Failed to create MQTT client` (error) | 创建客户端实例 |
| 3 | 注册事件回调 `mqtt_event_handler` | - | 监听所有 MQTT 事件 |
| 4 | `esp_mqtt_client_start(client)` | - | 启动连接流程 |

关键配置：

- `disable_auto_reconnect = false`
- `reconnect_timeout_ms = 10000`
- `hostname = BROKER_URL`
- `port = 1883`
- `transport = MQTT_TRANSPORT_OVER_TCP`
- `client_id = Lab_Sensor_Client`

---

## 2. 连接与事件流程（Connect & Event Phase）

### 2.1 事件回调 `mqtt_event_handler()`

| 事件 | 处理 | 状态变化 |
|------|------|---------|
| `MQTT_EVENT_CONNECTED` | 标记连接成功 | `Mqtt_Connect_State = 1` |
| `MQTT_EVENT_DISCONNECTED` | 标记连接断开 | `Mqtt_Connect_State = 0` |
| `MQTT_EVENT_SUBSCRIBED` | 打印订阅 ack | 无 |
| `MQTT_EVENT_UNSUBSCRIBED` | 打印取消订阅 ack | 无 |
| `MQTT_EVENT_PUBLISHED` | 打印发布 ack | 无 |
| `MQTT_EVENT_DATA` | 打印收到的 topic/data | 无 |
| `MQTT_EVENT_ERROR` | 打印 TLS/Socket 错误详情 | 无 |

典型日志：
```log
[INFO] Mqtt_Connect: MQTT_EVENT_CONNECTED
[INFO] Mqtt_Connect: MQTT_EVENT_DISCONNECTED
[INFO] Mqtt_Connect: MQTT_EVENT_PUBLISHED, msg_id=12
[INFO] Mqtt_Connect: MQTT_EVENT_ERROR
```

### 2.2 错误细节输出

当 `MQTT_EVENT_ERROR` 且错误类型为 TCP 传输时，输出：

- `esp_tls_last_esp_err`
- `esp_tls_stack_err`
- `esp_transport_sock_errno`
- `strerror(errno)`

用于快速区分证书/TLS、网络中断和 socket 错误。

---

## 3. 发布链路（Publish Pipeline）

### 3.1 业务任务入口

`Upload_Data_Task` 中处理上报：

1. 调用 `Mqtt_Is_Connectecd()` 检查连接状态
2. 从 `Mqtt_Data_Queue` 取 `SensorMessage_t`
3. 根据类型调用 JSON 打包函数
4. 调用 `Mqtt_Publish_Message(...)` 上报

当前主题：

- `sensors/water`
- `sensors/light`
- `sensors/pressure1`
- `sensors/pressure2`

### 3.2 发布确认

Broker 接收到发布并返回确认后，回调触发：

```log
[INFO] Mqtt_Connect: MQTT_EVENT_PUBLISHED, msg_id=...
```

---

## 4. 订阅链路（Subscribe Pipeline）

### 4.1 对外接口

- `Mqtt_Subscribe_Topic(topic, qos)`
- `Mqtt_Dissubscribe_Topic(topic)`

### 4.2 收到下行数据

`MQTT_EVENT_DATA` 时会打印：

```text
TOPIC=<topic>
DATA=<payload>
```

当前代码里“处理接收到的数据”逻辑预留，尚未实现具体业务分发。

---

## 5. 对外 API 总结

定义文件：`components/Mqtt_Connect/include/Mqtt_Connect.h`

| API | 功能 | 返回值 |
|-----|------|--------|
| `ConnectToMqtt(...)` | 初始化并启动 MQTT 客户端 | `void` |
| `Mqtt_Is_Connectecd()` | 查询连接状态 | `0/1` |
| `Mqtt_Subscribe_Topic(topic, qos)` | 订阅主题 | `void` |
| `Mqtt_Dissubscribe_Topic(topic)` | 取消订阅 | `void` |
| `Mqtt_Publish_Message(topic, data, len, qos, retain)` | 发布消息 | `void` |

---

## 6. 数据流图示

```text
app_main
  -> ConnectToMqtt
      -> init mqtt client
      -> register mqtt_event_handler
      -> start mqtt client
          -> MQTT_EVENT_CONNECTED

Upload_Data_Task
  -> receive from Mqtt_Data_Queue
  -> JSON_Pack*Data
  -> Mqtt_Publish_Message
      -> MQTT_EVENT_PUBLISHED
```

---

## 7. 调试日志等级建议

```c
esp_log_level_set("Mqtt_Connect", ESP_LOG_INFO);
esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
```

---

## 8. 常见故障排查

### 8.1 连不上 Broker

- 先看是否收到 `MQTT_EVENT_CONNECTED`。
- 核对 `hostname/port/用户名/密码`。
- 检查网络连通性与防火墙。

### 8.2 反复断开

- 看 `MQTT_EVENT_DISCONNECTED` 是否周期出现。
- 检查 WiFi 稳定性和 Broker 连接数限制。

### 8.3 发布无确认

- 检查是否有 `MQTT_EVENT_PUBLISHED`。
- 检查 QoS、主题权限和 Broker ACL。

### 8.4 下行消息无业务动作

- 当前只打印 topic/data，未做命令解析，需要在 `MQTT_EVENT_DATA` 分支补业务处理。

---

## 9. 总结

MQTT 模块链路可概括为：

```
ConnectToMqtt -> Event 回调维护状态 -> Upload_Data_Task 发布 -> MQTT_EVENT_PUBLISHED
```

模块已具备自动重连与错误细节日志，后续可增强点是下行消息解析和发布失败重试队列。
