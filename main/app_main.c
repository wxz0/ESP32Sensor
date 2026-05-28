#include <inttypes.h>

#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "Sensor.h"
#include "PressureSensor.h"
#include "Wifi_Connect.h"
#include "Mqtt_Connect.h"
#include "LED.h"
#include "KEY.h"
#include "Timestamp.h"
#include "App_Tasks.h"
#include "LCD_Driver.h"
#include "Usb_Storage.h"

#include "sdkconfig.h"

#ifndef CONFIG_MQTT_USERNAME
#define CONFIG_MQTT_USERNAME ""
#endif

#ifndef CONFIG_MQTT_PASSWORD
#define CONFIG_MQTT_PASSWORD ""
#endif

#ifndef CONFIG_BROKER_URL
#define CONFIG_BROKER_URL ""
#endif

static const char *TAG = "Lab_Sensor";

static void App_Nvs_Init(void)
{
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
}

static void Network_Upload_Task(void *pvParameters)
{
  (void)pvParameters;

  while (!Wifi_IS_Connected())
  {
    Wait_Wifi_Connected();
    if (!Wifi_IS_Connected())
    {
      vTaskDelay(pdMS_TO_TICKS(10000));
    }
  }

  SNTP_Sync_Time();
  ConnectToMqtt(CONFIG_BROKER_URL, CONFIG_MQTT_USERNAME, CONFIG_MQTT_PASSWORD);
  App_Start_Upload_Task();

  vTaskDelete(NULL);
}

void app_main(void)
{
  ESP_LOGI(TAG, "[APP] Startup..");
  ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
  ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

  App_Nvs_Init();

  WaterQualitySensor_init();
  LED_Init();
  KEY_Init();

  Wifi_Init();

  if (PressureSensor_Init() != ESP_OK)
  {
    ESP_LOGW(TAG, "压力传感器 BLE 初始化失败，请检查蓝牙配置和 MAC 地址");
  }

  esp_err_t lcd_ret = LCD_Driver_Init();

  if (lcd_ret != ESP_OK)
  {
    ESP_LOGE(TAG, "LCD init failed: %s; continue sensor tasks without UI", esp_err_to_name(lcd_ret));
  }
  
  Usb_Storage_Init();

  App_Create_Task();

  xTaskCreatePinnedToCore(Network_Upload_Task,
                          "Network_Upload_Task",
                          4096,
                          NULL,
                          3,
                          NULL,
                          0);
}
