#include <inttypes.h>

#include "esp_system.h"
#include "esp_log.h"

#include "Sensor.h"
#include "PressureSensor.h"
#include "wifi_connect.h"
#include "Mqtt_Connect.h"
#include "LED.h"
#include "KEY.h"
#include "Timestamp.h"
#include "App_Tasks.h"

#include "sdkconfig.h"

#ifndef CONFIG_WIFI_SSID
#define CONFIG_WIFI_SSID ""
#endif

#ifndef CONFIG_WIFI_PASSWORD
#define CONFIG_WIFI_PASSWORD ""
#endif

#ifndef CONFIG_MQTT_USERNAME
#define CONFIG_MQTT_USERNAME ""
#endif

#ifndef CONFIG_MQTT_PASSWORD
#define CONFIG_MQTT_PASSWORD ""
#endif

static const char *TAG = "Lab_Sensor";

void app_main(void)
{

  ESP_LOGI(TAG, "[APP] Startup..");
  ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
  ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

  Wifi_Init();
  WaterQualitySensor_init();
  if (PressureSensor_Init() != ESP_OK)
  {
    ESP_LOGW(TAG, "压力传感器 BLE 初始化失败，请检查蓝牙配置和 MAC 地址");
  }
  LED_Init();
  KEY_Init();

  ConnectToWifi(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
  ConnectToMqtt(CONFIG_BROKER_URL, CONFIG_MQTT_USERNAME, CONFIG_MQTT_PASSWORD);
  SNTP_Sync_Time();

  App_Create_Task();
  
}
