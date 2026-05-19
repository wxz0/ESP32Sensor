#include <stdio.h>
#include <string.h>
#include "Wifi_Connect.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "esp_log.h"

static const char *TAG = "Wifi_Connect";
uint8_t Wifi_Connect_State = 0; // 0: 未连接，1：已连接
/*
 ** @brief WiFi事件处理函数
 ** @param handler_args 事件处理函数参数
 ** @param base 事件基础
 ** @param event_id 事件ID
 ** @param event_data 事件数据

*/
static void wifi_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    if(base == WIFI_EVENT)
    {
        switch (event_id)
        {
            case WIFI_EVENT_STA_START:
                esp_wifi_connect();
                ESP_LOGI(TAG,"wifi已启动,正在尝试连接到路由器");
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG,"wifi已连接");
                Wifi_Connect_State = 1;
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                Wifi_Connect_State = 0;
                esp_wifi_connect();
                ESP_LOGW(TAG,"wifi断开连接");
                break;
        }
    }
    else if(base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t*)event_data;
        char ip[16];
        esp_ip4addr_ntoa(&event->ip_info.ip,ip,sizeof(ip));
        ESP_LOGI(TAG,"WiFi连接成功，获取到ip地址为：%s",ip);
    }

}

void Wifi_Init()
{
  esp_err_t ret = nvs_flash_init();                            // 尝试初始化NVS
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {  
    // 如果NVS分区有问题，就擦除并重新初始化
    ESP_ERROR_CHECK(nvs_flash_erase());                     // 擦除NVS分区
    ESP_ERROR_CHECK(nvs_flash_init());                      // 重新初始化NVS
  }
  /*初始化TCP/IP网络栈*/
  ESP_ERROR_CHECK(esp_netif_init());

  /*创建默认的事件循环*/
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  /*创建默认的STA接口*/
  esp_netif_create_default_wifi_sta();

  /*初始化wifi驱动*/
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  /*注册事件处理函数*/
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &wifi_event_handler,
                                                      NULL,
                                                      NULL));
  /*设置wifi工作模式为STA*/
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  
  ESP_LOGI(TAG,"wifi STA初始化完成");
  
}

uint8_t Wifi_IS_Connected(void)
{
    return Wifi_Connect_State;
}

void Wait_Wifi_Connected(void)
{
  uint8_t Try_Times = 0;
  for(Try_Times = 0; Try_Times < 5; Try_Times++)
  {
    ESP_LOGI(TAG,"等待WiFi连接...");
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    if(Wifi_IS_Connected() == 1)
    {
       break;
    }
  }      
  ESP_LOGI(TAG,"WiFi已连接");
}


void ConnectToWifi(const char* WIFI_SSID, const char* WIFI_PASW)
{
  /*设置wifi连接参数*/
   wifi_config_t wifi_config = {0};
   wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
   
   // 复制SSID到数组
   strncpy((char *)wifi_config.sta.ssid, (const char *)WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
   wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';
   
   // 复制密码到数组
   strncpy((char *)wifi_config.sta.password, (const char *)WIFI_PASW, sizeof(wifi_config.sta.password) - 1);
   wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';
   
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA,&wifi_config));
  
  ESP_ERROR_CHECK(esp_wifi_start());

  Wait_Wifi_Connected();
}

