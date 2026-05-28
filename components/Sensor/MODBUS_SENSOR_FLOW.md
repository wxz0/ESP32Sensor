# Modbus 传感器读取流程文档

## 概述
`Sensor` 模块基于 ESP-IDF `esp-modbus` 主站实现，通过 UART1 + RS485 半双工读取水质和光照传感器。参数读取由描述符表驱动，上层任务按周期调用读取接口并推送到 UI/MQTT 队列。

---

## 1. 初始化流程（Init Phase）

### 1.1 入口函数
```c
// main/app_main.c
WaterQualitySensor_init();
```

### 1.2 `WaterQualitySensor_init()` 核心流程

| 步骤 | 操作 | 输出日志 | 说明 |
|------|------|---------|------|
| 1 | 配置 `mb_communication_info_t` | - | 端口、RTU、波特率、超时 |
| 2 | `mbc_master_create_serial(&comm, &master_handle)` | `mb controller initialization fail...` | 创建主站控制器 |
| 3 | `uart_set_pin(1, 18, 17, 16, ...)` | `mb serial set pin failure...` | 配置 UART 引脚 |
| 4 | `mbc_master_start(master_handle)` | `mb controller start fail...` | 启动 Modbus 主站 |
| 5 | `uart_set_mode(..., UART_MODE_RS485_HALF_DUPLEX)` | `mb serial set mode failure...` | 设置 RS485 半双工 |
| 6 | `mbc_master_set_descriptor(...device_parameters...)` | `mb controller set descriptor fail...` | 注册参数描述符 |
| 7 | 初始化完成 | `Modbus 初始化完成` | 读取功能可用 |

关键通信参数：

- UART 端口：`1`
- 波特率：`9600`
- 模式：`MB_RTU`
- 数据位：`8`
- 停止位：`1`
- 校验：`None`

---

## 2. 参数模型与映射（Descriptor Phase）

### 2.1 从站地址

- 水质设备：`1`
- 光照设备：`2`、`3`、`4`、`5`

### 2.2 描述符表 `device_parameters[]`

每个参数项定义：

- `CID`
- 参数名 / 单位
- 从站地址
- 寄存器类型（Holding/Input）
- 起始寄存器
- 寄存器长度
- 数据类型（U16/U32）
- 权限（当前主要是读）

核心参数 CID：

- 水质：`CID_TEMP_C`、`CID_PH`、`CID_TDS`、`CID_ORP`、`CID_SALT_PCT`、`CID_SALT_PPT`、`CID_SG`、`CID_EC`、`CID_CL`
- pH 校准：`CID_CALIB_PH_401`、`CID_CALIB_PH_686`、`CID_CALIB_PH_918`
- 光照：`CID_LIGHT_1` ~ `CID_LIGHT_4`

---

## 3. 水质读取流程（Water Read Phase）

### 3.1 上层调用路径

`App_Tasks.c` 中 `WaterQuality_Sensor_Task` 每 15 秒调用：

```c
water_sensor_read_all(&Sensor_data);
```

### 3.2 `water_sensor_read_all()` 执行顺序

1. 参数检查：`out_data` 非空、`master_handle` 已初始化
2. 清空 `WaterQualityData_t`
3. 逐项读取 Modbus 参数
4. 原始值换算为物理量
5. 若任何一项失败，返回 `ESP_FAIL` 并打印失败项

转换规则：

- 温度：`raw / 10`
- PH：`raw / 100`
- TDS：`U32` 直接值
- ORP：`(int16_t)raw`
- 盐度百分比：`raw / 100`
- 盐度 PPT：`raw / 10`
- 比重 SG：`raw / 1000`
- 电导率 EC：`U32` 直接值
- 余氯 CL：`raw / 100`

失败日志示例：
```log
[INFO] Sensor: Failed to read temperature
[INFO] Sensor: Part of sensor data read failed
```

---

## 4. 光照读取流程（Light Read Phase）

### 4.1 上层调用路径

`App_Tasks.c` 中 `LightSensor_Task` 每 20 秒调用：

```c
light_sensor_read_all(&Sensor_data);
```

### 4.2 `light_sensor_read_all()` 执行顺序

1. 参数检查
2. 清空 `LightSensorData_t`
3. 依次读取 `CID_LIGHT_1~4`
4. 转换并填充 `lux_1~lux_4`

失败日志示例：
```log
[INFO] Sensor: Failed to read light2
[INFO] Sensor: Part of light sensor data read failed
```

---

## 5. pH 校准流程（Calibration Phase）

### 5.1 对外接口

- `water_sensor_calib_ph_4_01()`
- `water_sensor_calib_ph_6_86()`
- `water_sensor_calib_ph_9_18()`

### 5.2 公共校准逻辑

底层统一调用 `perform_ph_calibration_logic(cid, desc)`：

1. 写寄存器值 `1`（开始校准）
2. 延时 `40s`
3. 写寄存器值 `0`（结束校准）
4. 打印校准完成

示例日志：
```log
[INFO] Sensor: [PH 6.86] Start Calibration...
[INFO] Sensor: [PH 6.86] Waiting 40 seconds...
[INFO] Sensor: [PH 6.86] Calibration Completed Successfully.
```

注意：校准函数会阻塞当前任务 40 秒。

---

## 6. 对外 API 总结

定义文件：`components/Sensor/include/Sensor.h`

| API | 功能 | 返回值 |
|-----|------|--------|
| `WaterQualitySensor_init()` | 初始化 Modbus 主站 | `ESP_OK` / `ESP_ERR_*` |
| `water_sensor_read_all(out)` | 读取水质数据 | `ESP_OK` / `ESP_FAIL` / `ESP_ERR_*` |
| `light_sensor_read_all(out)` | 读取光照数据 | `ESP_OK` / `ESP_FAIL` / `ESP_ERR_*` |
| `water_sensor_calib_ph_4_01()` | pH 4.01 校准 | `ESP_OK` / `ESP_ERR_*` |
| `water_sensor_calib_ph_6_86()` | pH 6.86 校准 | `ESP_OK` / `ESP_ERR_*` |
| `water_sensor_calib_ph_9_18()` | pH 9.18 校准 | `ESP_OK` / `ESP_ERR_*` |

说明：`read_light_address()` 在头文件有声明，但 `Sensor.c` 里当前实现被注释，尚不可用。

---

## 7. 数据流图示

```text
app_main
  -> WaterQualitySensor_init
      -> create/start modbus master
      -> set rs485 mode
      -> load descriptor table

WaterQuality_Sensor_Task (15s)
  -> water_sensor_read_all
      -> mbc_master_get_parameter by CID
      -> convert raw value
      -> send queue for UI/MQTT

LightSensor_Task (20s)
  -> light_sensor_read_all
      -> read CID_LIGHT_1..4
      -> send queue for UI/MQTT
```

---

## 8. 调试日志等级建议

```c
esp_log_level_set("Sensor", ESP_LOG_INFO);
```

链路异常定位阶段可临时设置 `ESP_LOG_DEBUG`。

---

## 9. 常见故障排查

### 9.1 初始化失败

- 检查 UART 引脚映射：TX=18、RX=17、RTS=16。
- 检查 RS485 收发器 DE/RE 控制和供电。

### 9.2 全部读取失败

- 检查波特率/串口格式（当前 9600, 8N1, 无校验）。
- 检查 A/B 线是否接反。
- 检查从站地址是否与代码配置一致。

### 9.3 部分参数失败

- 根据 `Failed to read ...` 定位具体 CID。
- 检查对应传感器寄存器支持情况。

### 9.4 校准导致任务卡住

- 校准逻辑内置 40 秒阻塞延时。
- 建议在独立任务或维护窗口执行校准。

---

## 10. 总结

Modbus 模块核心链路：

```
WaterQualitySensor_init -> 周期读取 water/light -> 原始值转换 -> 推送上层队列
```

模块已具备稳定读取与校准能力，后续可补充非阻塞校准和 `read_light_address()` 实现。
