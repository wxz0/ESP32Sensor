# WiFi 连接流程文档

## 概述
WiFi 模块通过 `Wifi_Connect` 组件完成 STA 联网，采用 `WIFI_EVENT` 和 `IP_EVENT` 事件回调维护连接状态。上层在 `app_main()` 中按 `Wifi_Init() -> ConnectToWifi()` 顺序完成初始化和入网。

---

## 1. 初始化流程（Init Phase）

### 1.1 入口函数
```c
// main/app_main.c
Wifi_Init();
ConnectToWifi(WIFI_SSID, WIFI_PASW);
```

### 1.2 `Wifi_Init()` 核心流程

| 步骤 | 操作 | 输出日志 | 说明 |
|------|------|---------|------|
| 1 | `nvs_flash_init()` | - | 初始化 NVS |
| 2 | NVS 异常时擦除并重建 | - | 处理 `ESP_ERR_NVS_NO_FREE_PAGES` / `ESP_ERR_NVS_NEW_VERSION_FOUND` |
| 3 | `esp_netif_init()` | - | 初始化 TCP/IP 网络栈 |
| 4 | `esp_event_loop_create_default()` | - | 创建默认事件循环 |
| 5 | `esp_netif_create_default_wifi_sta()` | - | 创建 STA 网络接口 |
| 6 | `esp_wifi_init(&cfg)` | - | 初始化 WiFi 驱动 |
| 7 | 注册事件回调 `wifi_event_handler` | - | 处理连接状态和 IP 事件 |
| 8 | `esp_wifi_set_mode(WIFI_MODE_STA)` | `wifi STA初始化完成` | 配置为 STA 模式 |

---

## 2. 入网流程（Connect Phase）

### 2.1 配置并启动连接

调用 `ConnectToWifi(WIFI_SSID, WIFI_PASW)` 后执行：

1. 构造 `wifi_config_t` 并置零
2. 设置 `wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK`
3. 拷贝 SSID / Password 到 `wifi_config.sta`
4. `esp_wifi_set_config(WIFI_IF_STA, &wifi_config)`
5. `esp_wifi_start()`
6. 进入 `Wait_Wifi_Connected()` 等待连接

### 2.2 `Wait_Wifi_Connected()` 等待策略

- 最多轮询 5 次
- 每次延时 3 秒
- 通过 `Wifi_IS_Connected()` 读取 `Wifi_Connect_State`

注意：当前代码在循环结束后固定打印 `WiFi已连接`，即使未连接也会出现该日志，建议结合事件日志一起判断。

---

## 3. 事件驱动状态机（Event State Machine）

### 3.1 `wifi_event_handler()` 处理逻辑

| 事件 | 处理 | 状态变化 |
|------|------|---------|
| `WIFI_EVENT_STA_START` | 调用 `esp_wifi_connect()` 自动发起连接 | 无 |
| `WIFI_EVENT_STA_CONNECTED` | 打印连接成功日志 | `Wifi_Connect_State = 1` |
| `WIFI_EVENT_STA_DISCONNECTED` | 置未连接并立即重连 | `Wifi_Connect_State = 0` |
| `IP_EVENT_STA_GOT_IP` | 解析并打印 IPv4 地址 | 无 |

典型日志：
```log
[INFO] Wifi_Connect: wifi已启动,正在尝试连接到路由器
[INFO] Wifi_Connect: wifi已连接
[WARN] Wifi_Connect: wifi断开连接
[INFO] Wifi_Connect: WiFi连接成功，获取到ip地址为：192.168.x.x
```

---

## 4. 连接状态读取（Status Query）

对外状态接口：

- `uint8_t Wifi_IS_Connected(void)`

返回值约定：

- `0`: 未连接
- `1`: 已连接（在 `WIFI_EVENT_STA_CONNECTED` 置位）

---

## 5. 与上层模块关系（Upper Layer Integration）

### 5.1 在 `app_main` 中的调用顺序

```text
Wifi_Init
  -> ConnectToWifi
     -> Wait_Wifi_Connected
        -> ConnectToMqtt
           -> App_Create_Task
```

WiFi 是 MQTT 连接与数据上报的前置条件，断网会直接影响 `Mqtt_Connect` 模块连接稳定性。

---

## 6. 对外 API 总结

定义文件：`components/Wifi_Connect/include/Wifi_Connect.h`

| API | 功能 | 返回值 |
|-----|------|--------|
| `Wifi_Init()` | 初始化 WiFi 子系统 | `void` |
| `ConnectToWifi(ssid, pass)` | 设置参数并启动 STA 连接 | `void` |
| `Wifi_IS_Connected()` | 获取连接状态 | `0/1` |

---

## 7. 数据流图示

```text
app_main
  -> Wifi_Init
     -> NVS init / netif init / event loop / wifi init / set STA mode
  -> ConnectToWifi
     -> set wifi config
     -> esp_wifi_start
        -> WIFI_EVENT_STA_START
           -> esp_wifi_connect
        -> WIFI_EVENT_STA_CONNECTED
           -> Wifi_Connect_State = 1
        -> IP_EVENT_STA_GOT_IP
           -> print ip address
```

---

## 8. 调试日志等级建议

```c
esp_log_level_set("Wifi_Connect", ESP_LOG_INFO);
```

排障阶段可临时提高到 `ESP_LOG_DEBUG` 观察更多事件细节。

---

## 9. 常见故障排查

### 9.1 无法拿到 IP

- 检查是否出现 `WIFI_EVENT_STA_CONNECTED`。
- 检查是否收到 `IP_EVENT_STA_GOT_IP`。
- 确认路由器 DHCP 可用。

### 9.2 频繁断线

- 观察 `wifi断开连接` 日志频率。
- 检查供电、信号强度和认证配置。

### 9.3 连接等待误判

- 由于 `Wait_Wifi_Connected()` 末尾固定打印连接成功，建议以事件日志或 `Wifi_IS_Connected()` 实时值作为准确信号。

---

## 10. 总结

WiFi 模块核心链路为：

```
Wifi_Init -> ConnectToWifi -> Event 回调更新连接状态 -> 获取 IP -> 上层启动 MQTT
```

该模块逻辑清晰，主要风险点在于等待函数的日志语义与实际状态不完全一致，排障时请优先参考事件日志。
