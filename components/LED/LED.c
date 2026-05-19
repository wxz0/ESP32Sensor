#include <stdio.h>
#include "LED.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"

#define LED1_GPIO_NUM  38
#define LED2_GPIO_NUM  39
#define LED1_GPIO_PIN_MASK  (1ULL<<LED1_GPIO_NUM)
#define LED2_GPIO_PIN_MASK  (1ULL<<LED2_GPIO_NUM)
static const char *TAG = "LED";

void LED_Init(void)
{
  esp_err_t ret = ESP_FAIL;
  gpio_config_t LED_GPIO_Config = {
    .pin_bit_mask = (LED1_GPIO_PIN_MASK|LED2_GPIO_PIN_MASK),
    .mode = GPIO_MODE_OUTPUT,
    .intr_type = GPIO_INTR_DISABLE
  };
  ret = gpio_config(&LED_GPIO_Config);
  if(ret != ESP_OK)
  {
    ESP_LOGE(TAG, "LED GPIO 配置失败，错误码：%d", ret);
  }
  gpio_set_level(LED1_GPIO_NUM, 0);
  gpio_set_level(LED2_GPIO_NUM, 0);
  ESP_LOGI(TAG, "LED 初始化完成");
}

void LED_ON(LED_x n)
{
  switch(n)
  {
    case LED1:
        gpio_set_level(LED1_GPIO_NUM, 1);
        ESP_LOGI(TAG, "LED1 点亮");
        break;
        case LED2:
        gpio_set_level(LED2_GPIO_NUM, 1);
        ESP_LOGI(TAG, "LED2 点亮");
        break;
        default:
        ESP_LOGW(TAG, "无效的 LED 编号：%d", n);
        break;
  }
}

void LED_OFF(LED_x n)
{
  switch(n)
  {
    case LED1:
        gpio_set_level(LED1_GPIO_NUM, 0);
        ESP_LOGI(TAG, "LED1 熄灭");
        break;
        case LED2:
        gpio_set_level(LED2_GPIO_NUM, 0);
        ESP_LOGI(TAG, "LED2 熄灭");
        break;
        default:
        ESP_LOGW(TAG, "无效的 LED 编号：%d", n);
        break;
  }
}