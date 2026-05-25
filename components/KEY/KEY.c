#include <stdio.h>
#include "KEY.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"


#define KEY1_GPIO_NUM 13
#define KEY2_GPIO_NUM 14
#define KEY3_GPIO_NUM 21
#define KEY4_GPIO_NUM 47
#define KEY1_GPIO_PIN_MASK  (1ULL<<KEY1_GPIO_NUM)
#define KEY2_GPIO_PIN_MASK  (1ULL<<KEY2_GPIO_NUM)
#define KEY3_GPIO_PIN_MASK  (1ULL<<KEY3_GPIO_NUM)
#define KEY4_GPIO_PIN_MASK  (1ULL<<KEY4_GPIO_NUM)

static const char *TAG = "KEY";

Key_State_t KeyNum = 0;

void KEY_Init(void)
{
  esp_err_t ret = ESP_FAIL;
  gpio_config_t KEY_GPIO_Config = {
    .pin_bit_mask = (KEY1_GPIO_PIN_MASK|KEY2_GPIO_PIN_MASK|KEY3_GPIO_PIN_MASK|KEY4_GPIO_PIN_MASK),
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .mode = GPIO_MODE_INPUT,
    .intr_type = GPIO_INTR_DISABLE
  };
  ret = gpio_config(&KEY_GPIO_Config);
  if(ret != ESP_OK)
  {
    ESP_LOGE(TAG, "KEY GPIO 配置失败，错误码：%d", ret);
  }
  ESP_LOGI(TAG, "LED 初始化完成");
}

Key_State_t KEY_GetState(void)
{
  Key_State_t key_state_cur = 0x00;
  static Key_State_t key_state_pre = 0x00;
  key_state_pre = key_state_pre << 4;
  gpio_get_level(KEY1_GPIO_NUM)?(key_state_cur|=0x01):(key_state_cur&=~0x01);
  gpio_get_level(KEY2_GPIO_NUM)?(key_state_cur|=0x02):(key_state_cur&=~0x02);
  gpio_get_level(KEY3_GPIO_NUM)?(key_state_cur|=0x04):(key_state_cur&=~0x04);
  gpio_get_level(KEY4_GPIO_NUM)?(key_state_cur|=0x08):(key_state_cur&=~0x08);
  key_state_pre |= key_state_cur;
  return key_state_pre;
}

KEY_x KEY_GetNum(void)
{
   Key_State_t key_state = KEY_GetState();
   if((key_state & 0x11)== 0x10)return KEY1;
   if((key_state & 0x22)== 0x20)return KEY2;
   if((key_state & 0x44)== 0x40)return KEY3;
   if((key_state & 0x88)== 0x80)return KEY4;
   return 0;
}  