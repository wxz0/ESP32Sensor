#include "LCD_Driver.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "driver/spi_master.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "setup_ui.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "LCD_Driver";

/* Screen objects from UI component */
extern lv_obj_t * screen_data_show_1;
extern lv_obj_t * screen_show_2;
extern lv_obj_t * screen_wait_ec_correct;
extern lv_obj_t * screen_wait_ph_correct;
extern lv_obj_t * screen_wait_orp_correct;

/* Label objects for data updates - from setup_screen_data_show_1.c */
extern lv_obj_t * screen_data_show_1_label_pressure1_data;
extern lv_obj_t * screen_data_show_1_label_pressure2_data;
extern lv_obj_t * screen_data_show_1_label_text_data;
extern lv_obj_t * screen_data_show_1_label_ph_data;
extern lv_obj_t * screen_data_show_1_label_tds_data;
extern lv_obj_t * screen_data_show_1_label_orp_data;
extern lv_obj_t * screen_data_show_1_label_sg_data;
extern lv_obj_t * screen_data_show_1_label_ec_data;
extern lv_obj_t * screen_data_show_1_label_salt1_data;
extern lv_obj_t * screen_data_show_1_label_salt2_data;

/* Label objects for data updates - from setup_screen_show_2.c */
extern lv_obj_t * screen_show_2_label_lux1_data;
extern lv_obj_t * screen_show_2_label_lux2_data;
extern lv_obj_t * screen_show_2_label_lux3_data;
extern lv_obj_t * screen_show_2_label_lux_data;

static esp_lcd_panel_handle_t s_panel_handle = NULL;
static SemaphoreHandle_t s_lvgl_mutex = NULL;

/* LVGL flush callback */
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
    esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
    lv_display_flush_ready(disp);
}

static void lvgl_tick_cb(void *arg)
{
    lv_tick_inc(CONFIG_LVGL_TICK_PERIOD_MS);
}

/* LVGL handler task */
static void lvgl_task(void *arg)
{
    ESP_LOGI(TAG, "LVGL task started");
    while (1) {
        if (xSemaphoreTake(s_lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            lv_timer_handler();
            xSemaphoreGive(s_lvgl_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void lcd_lock(void)
{
    xSemaphoreTake(s_lvgl_mutex, portMAX_DELAY);
}

static void lcd_unlock(void)
{
    xSemaphoreGive(s_lvgl_mutex);
}

void LCD_Driver_Init(void)
{
    ESP_LOGI(TAG, "Initializing SPI bus...");
    spi_bus_config_t bus_cfg = {
        .sclk_io_num = CONFIG_LCD_PIN_SCK,
        .mosi_io_num = CONFIG_LCD_PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = CONFIG_LCD_H_RES * CONFIG_LCD_V_RES * sizeof(uint16_t),
    };
    spi_host_device_t spi_host = (CONFIG_LCD_SPI_HOST == 2) ? SPI2_HOST : SPI3_HOST;
    ESP_ERROR_CHECK(spi_bus_initialize(spi_host, &bus_cfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Creating LCD panel IO...");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = CONFIG_LCD_PIN_CS,
        .dc_gpio_num = CONFIG_LCD_PIN_DC,
        .spi_mode = 0,
        .pclk_hz = CONFIG_LCD_SPI_CLOCK_HZ,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = CONFIG_LCD_SPI_CMD_BITS,
        .lcd_param_bits = CONFIG_LCD_SPI_PARAM_BITS,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)spi_host, &io_config, &io_handle));

    ESP_LOGI(TAG, "Creating ST7789 panel...");
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = CONFIG_LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_cfg, &s_panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel_handle));
    esp_lcd_panel_invert_color(s_panel_handle, true);
    esp_lcd_panel_set_gap(s_panel_handle, 0, 0);
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel_handle, true));

    ESP_LOGI(TAG, "Initializing LVGL...");
    lv_init();

    /* Create mutex for LVGL thread safety */
    s_lvgl_mutex = xSemaphoreCreateMutex();
    assert(s_lvgl_mutex);

    /* Allocate partial display buffer (20 lines) in internal RAM */
    ESP_LOGI(TAG, "Allocating display buffer...");
    size_t buf_size = CONFIG_LCD_H_RES * 20 * sizeof(lv_color_t);
    lv_color_t *buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    assert(buf1);

    /* Create display and set buffers */
    lv_display_t *disp = lv_display_create(CONFIG_LCD_H_RES, CONFIG_LCD_V_RES);
    lv_display_set_flush_cb(disp, lvgl_flush_cb);
    lv_display_set_user_data(disp, s_panel_handle);
    lv_display_set_buffers(disp, buf1, NULL, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

    ESP_LOGI(TAG, "Starting LVGL tasks...");
    const esp_timer_create_args_t tick_timer_args = {
        .callback = lvgl_tick_cb,
        .name = "lvgl_tick",
    };
    esp_timer_handle_t tick_timer;
    ESP_ERROR_CHECK(esp_timer_create(&tick_timer_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, CONFIG_LVGL_TICK_PERIOD_MS * 1000));
    xTaskCreatePinnedToCore(lvgl_task, "lvgl_task", CONFIG_LVGL_TASK_STACK_SIZE, NULL, CONFIG_LVGL_TASK_PRIORITY, NULL, CONFIG_LVGL_TASK_CORE);

    ESP_LOGI(TAG, "Creating UI screens...");
    lcd_lock();
    setupUi();
    lcd_unlock();

    ESP_LOGI(TAG, "LCD initialized (%dx%d)", CONFIG_LCD_H_RES, CONFIG_LCD_V_RES);
}

void lcd_switch_to_main(void)
{
    if (screen_data_show_1 == NULL) {
        lcd_lock();
        setup_screen_data_show_1();
        lcd_unlock();
    }
    lcd_lock();
    lv_scr_load(screen_data_show_1);
    lcd_unlock();
}

void lcd_switch_to_light(void)
{
    if (screen_show_2 == NULL) {
        lcd_lock();
        setup_screen_show_2();
        lcd_unlock();
    }
    lcd_lock();
    lv_scr_load(screen_show_2);
    lcd_unlock();
}

static void update_label_float(lv_obj_t *label, const char *fmt, float value)
{
    if (label == NULL) return;
    char buf[32];
    snprintf(buf, sizeof(buf), fmt, value);
    lv_label_set_text(label, buf);
}

static void update_label_u32(lv_obj_t *label, const char *fmt, uint32_t value)
{
    if (label == NULL) return;
    char buf[32];
    snprintf(buf, sizeof(buf), fmt, (unsigned long)value);
    lv_label_set_text(label, buf);
}

static void update_label_i32(lv_obj_t *label, const char *fmt, int32_t value)
{
    if (label == NULL) return;
    char buf[32];
    snprintf(buf, sizeof(buf), fmt, (long)value);
    lv_label_set_text(label, buf);
}

void lcd_ui_update_sensor_data(const WaterQualityData_t *water,
                               const LightSensorData_t *light,
                               const PressureSensorData_t *press1,
                               const PressureSensorData_t *press2)
{
    lcd_lock();

    if (press1) {
        update_label_float(screen_data_show_1_label_pressure1_data, "%.0f", press1->pressure_cur);
    }
    if (press2) {
        update_label_float(screen_data_show_1_label_pressure2_data, "%.0f", press2->pressure_cur);
    }
    if (water) {
        update_label_float(screen_data_show_1_label_text_data, "%.1f", water->temp);
        update_label_float(screen_data_show_1_label_ph_data, "%.2f", water->ph);
        update_label_u32(screen_data_show_1_label_tds_data, "%lu", water->tds);
        update_label_i32(screen_data_show_1_label_orp_data, "%ld", water->orp);
        update_label_float(screen_data_show_1_label_sg_data, "%.3f", water->sg);
        update_label_u32(screen_data_show_1_label_ec_data, "%lu", water->ec);
        update_label_float(screen_data_show_1_label_salt1_data, "%.1f", water->saltppt);
        update_label_float(screen_data_show_1_label_salt2_data, "%.1f", water->salt);
    }
    if (light) {
        update_label_float(screen_show_2_label_lux1_data, "%.0f", light->lux_1);
        update_label_float(screen_show_2_label_lux2_data, "%.0f", light->lux_2);
        update_label_float(screen_show_2_label_lux3_data, "%.0f", light->lux_3);
        update_label_float(screen_show_2_label_lux_data, "%.0f", light->lux_4);
    }

    lcd_unlock();
}

void lcd_show_calibration(const char *name)
{
    lcd_lock();

    if (strcmp(name, "EC") == 0) {
        if (screen_wait_ec_correct == NULL) setup_screen_wait_ec_correct();
        lv_scr_load(screen_wait_ec_correct);
    } else if (strcmp(name, "PH") == 0) {
        if (screen_wait_ph_correct == NULL) setup_screen_wait_ph_correct();
        lv_scr_load(screen_wait_ph_correct);
    } else if (strcmp(name, "ORP") == 0) {
        if (screen_wait_orp_correct == NULL) setup_screen_wait_orp_correct();
        lv_scr_load(screen_wait_orp_correct);
    }

    lcd_unlock();
}