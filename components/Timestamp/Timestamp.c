#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include "Timestamp.h"
#include "esp_sntp.h"

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"


static const char *TAG = "SNTP";

#ifndef CONFIG_SNTP_SERVER_0
#define CONFIG_SNTP_SERVER_0 "ntp1.aliyun.com"
#endif

#ifndef CONFIG_SNTP_SERVER_1
#define CONFIG_SNTP_SERVER_1 "cn.pool.ntp.org"
#endif

#ifndef CONFIG_SNTP_TIMEZONE
#define CONFIG_SNTP_TIMEZONE "CST-8"
#endif

#ifndef CONFIG_SNTP_MAX_RETRIES
#define CONFIG_SNTP_MAX_RETRIES 15
#endif

#ifndef CONFIG_SNTP_RETRY_DELAY_MS
#define CONFIG_SNTP_RETRY_DELAY_MS 2000
#endif

esp_err_t SNTP_Sync_Time(void)
{
  esp_err_t ret = ESP_OK;
  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, CONFIG_SNTP_SERVER_0);
  esp_sntp_setservername(1, CONFIG_SNTP_SERVER_1);
  
  
  esp_sntp_init();
  /*设置时区*/
  setenv("TZ", CONFIG_SNTP_TIMEZONE, 1);
  ESP_LOGI(TAG, "Initializing SNTP");

  int retry = 0;
  const int retry_period_ms = CONFIG_SNTP_RETRY_DELAY_MS;
  const int max_retries = CONFIG_SNTP_MAX_RETRIES;

  while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET) 
  {
      if (retry >= max_retries) 
      {
          ESP_LOGE(TAG, "SNTP 同步超时");
          ret = ESP_FAIL;
          break;
      }
      if (retry % 20 == 0) 
      {
          ESP_LOGI(TAG, "等待SNTP同步中.....(%d/%d)", retry, max_retries);
      }
      
      vTaskDelay(pdMS_TO_TICKS(retry_period_ms));
      retry++;
  }
  if(ret == ESP_OK)
  {
    ESP_LOGI(TAG, "SNTP同步成功");
  }
  else
  {
    ESP_LOGI(TAG,"SNTP同步失败");
  }
  return ret;
}

time_t SNTP_Get_Timestamp(void)
{
  time_t now;
  time(&now);
  return now;
}
