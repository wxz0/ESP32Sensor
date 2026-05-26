#include "LCD_Driver.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "sdkconfig.h"
#include "setup_ui.h"

static const char *TAG = "LCD_Driver";

extern lv_obj_t *screen_data_show_1;
extern lv_obj_t *screen_show_2;
extern lv_obj_t *screen_wait_ec_correct;
extern lv_obj_t *screen_wait_ph_correct;
extern lv_obj_t *screen_wait_orp_correct;

extern lv_obj_t *screen_wait_ec_correct_label_name;
extern lv_obj_t *screen_wait_ec_correct_label_time;
extern lv_obj_t *screen_wait_ec_correct_label_status;
extern lv_obj_t *screen_wait_ec_correct_bar;
extern lv_obj_t *screen_wait_ph_correct_label_name;
extern lv_obj_t *screen_wait_ph_correct_label_time;
extern lv_obj_t *screen_wait_ph_correct_label_status;
extern lv_obj_t *screen_wait_ph_correct_bar;
extern lv_obj_t *screen_wait_orp_correct_label_name;
extern lv_obj_t *screen_wait_orp_correct_label_time;
extern lv_obj_t *screen_wait_orp_correct_label_status;
extern lv_obj_t *screen_wait_orp_correct_bar;

extern lv_obj_t *screen_data_show_1_label_pressure1_data;
extern lv_obj_t *screen_data_show_1_label_pressure2_data;
extern lv_obj_t *screen_data_show_1_label_text_data;
extern lv_obj_t *screen_data_show_1_label_ph_data;
extern lv_obj_t *screen_data_show_1_label_tds_data;
extern lv_obj_t *screen_data_show_1_label_orp_data;
extern lv_obj_t *screen_data_show_1_label_sg_data;
extern lv_obj_t *screen_data_show_1_label_ec_data;
extern lv_obj_t *screen_data_show_1_label_salt1_data;
extern lv_obj_t *screen_data_show_1_label_salt2_data;

extern lv_obj_t *screen_show_2_label_lux1_data;
extern lv_obj_t *screen_show_2_label_lux2_data;
extern lv_obj_t *screen_show_2_label_lux3_data;
extern lv_obj_t *screen_show_2_label_lux_data;

static esp_lcd_panel_handle_t s_panel = NULL;
static lv_display_t *s_display = NULL;
static SemaphoreHandle_t s_lvgl_mutex = NULL;

static bool lcd_flush_done_cb(esp_lcd_panel_io_handle_t panel_io,
                              esp_lcd_panel_io_event_data_t *edata,
                              void *user_ctx)
{
    (void)panel_io;
    (void)edata;
    (void)user_ctx;
    lv_display_flush_ready(s_display);
    return false;
}

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    lv_draw_sw_rgb565_swap(px_map, lv_area_get_size(area));
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(s_panel,
                                              area->x1,
                                              area->y1,
                                              area->x2 + 1,
                                              area->y2 + 1,
                                              px_map));
}

static void lvgl_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(CONFIG_LVGL_TICK_PERIOD_MS);
}

static void lvgl_task(void *arg)
{
    (void)arg;

    while(1) {
        if(xSemaphoreTake(s_lvgl_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
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
    ESP_LOGI(TAG, "Initialize LCD");

    spi_host_device_t spi_host = (CONFIG_LCD_SPI_HOST == 2) ? SPI2_HOST : SPI3_HOST;
    spi_bus_config_t bus_cfg = {
        .sclk_io_num = CONFIG_LCD_PIN_SCK,
        .mosi_io_num = CONFIG_LCD_PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = CONFIG_LCD_H_RES * 40 * sizeof(lv_color_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(spi_host, &bus_cfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = CONFIG_LCD_PIN_CS,
        .dc_gpio_num = CONFIG_LCD_PIN_DC,
        .spi_mode = 0,
        .pclk_hz = CONFIG_LCD_SPI_CLOCK_HZ,
        .trans_queue_depth = 4,
        .lcd_cmd_bits = CONFIG_LCD_SPI_CMD_BITS,
        .lcd_param_bits = CONFIG_LCD_SPI_PARAM_BITS,
        .on_color_trans_done = lcd_flush_done_cb,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)spi_host,
                                             &io_cfg,
                                             &io));

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = CONFIG_LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io, &panel_cfg, &s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(s_panel, 0, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));

    lv_init();

    s_lvgl_mutex = xSemaphoreCreateMutex();
    assert(s_lvgl_mutex != NULL);

    size_t draw_buf_size = CONFIG_LCD_H_RES * 40 * sizeof(lv_color_t);
    lv_color_t *draw_buf = heap_caps_malloc(draw_buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    assert(draw_buf != NULL);

    s_display = lv_display_create(CONFIG_LCD_H_RES, CONFIG_LCD_V_RES);
    lv_display_set_color_format(s_display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(s_display, lvgl_flush_cb);
    lv_display_set_buffers(s_display, draw_buf, NULL, draw_buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

    const esp_timer_create_args_t tick_args = {
        .callback = lvgl_tick_cb,
        .name = "lvgl_tick",
    };
    esp_timer_handle_t tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, CONFIG_LVGL_TICK_PERIOD_MS * 1000));

    lcd_lock();
    setupUi();
    lcd_unlock();

    xTaskCreatePinnedToCore(lvgl_task,
                            "lvgl_task",
                            CONFIG_LVGL_TASK_STACK_SIZE,
                            NULL,
                            CONFIG_LVGL_TASK_PRIORITY,
                            NULL,
                            CONFIG_LVGL_TASK_CORE);

    ESP_LOGI(TAG, "LCD ready: %dx%d", CONFIG_LCD_H_RES, CONFIG_LCD_V_RES);
}

void lcd_switch_to_main(void)
{
    lcd_lock();
    if(screen_data_show_1 == NULL) {
        setup_screen_data_show_1();
    }
    lv_scr_load(screen_data_show_1);
    lcd_unlock();
}

void lcd_switch_to_light(void)
{
    lcd_lock();
    if(screen_show_2 == NULL) {
        setup_screen_show_2();
    }
    lv_scr_load(screen_show_2);
    lcd_unlock();
}

static void update_label_float(lv_obj_t *label, const char *fmt, float value)
{
    if(label == NULL) {
        return;
    }

    char text[32];
    snprintf(text, sizeof(text), fmt, value);
    lv_label_set_text(label, text);
}

static void update_label_u32(lv_obj_t *label, const char *fmt, uint32_t value)
{
    if(label == NULL) {
        return;
    }

    char text[32];
    snprintf(text, sizeof(text), fmt, (unsigned long)value);
    lv_label_set_text(label, text);
}

static void update_label_i32(lv_obj_t *label, const char *fmt, int32_t value)
{
    if(label == NULL) {
        return;
    }

    char text[32];
    snprintf(text, sizeof(text), fmt, (long)value);
    lv_label_set_text(label, text);
}

void lcd_ui_update_sensor_data(const WaterQualityData_t *water,
                               const LightSensorData_t *light,
                               const PressureSensorData_t *press1,
                               const PressureSensorData_t *press2)
{
    lcd_lock();

    if(press1 != NULL) {
        update_label_float(screen_data_show_1_label_pressure1_data, "%.0f", press1->pressure_cur);
    }

    if(press2 != NULL) {
        update_label_float(screen_data_show_1_label_pressure2_data, "%.0f", press2->pressure_cur);
    }

    if(water != NULL) {
        update_label_float(screen_data_show_1_label_text_data, "%.1f", water->temp);
        update_label_float(screen_data_show_1_label_ph_data, "%.2f", water->ph);
        update_label_u32(screen_data_show_1_label_tds_data, "%lu", water->tds);
        update_label_i32(screen_data_show_1_label_orp_data, "%ld", water->orp);
        update_label_float(screen_data_show_1_label_sg_data, "%.3f", water->sg);
        update_label_u32(screen_data_show_1_label_ec_data, "%lu", water->ec);
        update_label_float(screen_data_show_1_label_salt1_data, "%.1f", water->saltppt);
        update_label_float(screen_data_show_1_label_salt2_data, "%.1f", water->salt);
    }

    if(light != NULL) {
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

    if(name != NULL && strncmp(name, "PH", 2) == 0) {
        if(screen_wait_ph_correct == NULL) {
            setup_screen_wait_ph_correct();
        }
        lv_scr_load(screen_wait_ph_correct);
    } else if(name != NULL && strncmp(name, "ORP", 3) == 0) {
        if(screen_wait_orp_correct == NULL) {
            setup_screen_wait_orp_correct();
        }
        lv_scr_load(screen_wait_orp_correct);
    } else {
        if(screen_wait_ec_correct == NULL) {
            setup_screen_wait_ec_correct();
        }
        lv_scr_load(screen_wait_ec_correct);
    }

    lcd_unlock();
}

void lcd_update_calibration_status(const char *name,
                                   int remaining_sec,
                                   int total_sec,
                                   const char *status)
{
    lv_obj_t *name_label = screen_wait_ec_correct_label_name;
    lv_obj_t *time_label = screen_wait_ec_correct_label_time;
    lv_obj_t *status_label = screen_wait_ec_correct_label_status;
    lv_obj_t *bar = screen_wait_ec_correct_bar;
    char time_text[24];
    int progress = 0;

    if(name != NULL && strncmp(name, "PH", 2) == 0) {
        name_label = screen_wait_ph_correct_label_name;
        time_label = screen_wait_ph_correct_label_time;
        status_label = screen_wait_ph_correct_label_status;
        bar = screen_wait_ph_correct_bar;
    } else if(name != NULL && strncmp(name, "ORP", 3) == 0) {
        name_label = screen_wait_orp_correct_label_name;
        time_label = screen_wait_orp_correct_label_time;
        status_label = screen_wait_orp_correct_label_status;
        bar = screen_wait_orp_correct_bar;
    }

    if(total_sec > 0) {
        int elapsed = total_sec - remaining_sec;
        if(elapsed < 0) {
            elapsed = 0;
        } else if(elapsed > total_sec) {
            elapsed = total_sec;
        }
        progress = (elapsed * 100) / total_sec;
    }

    if(remaining_sec < 0) {
        remaining_sec = 0;
    }

    snprintf(time_text, sizeof(time_text), "%d s left", remaining_sec);

    lcd_lock();
    if(name_label != NULL && name != NULL) {
        lv_label_set_text(name_label, name);
    }
    if(time_label != NULL) {
        lv_label_set_text(time_label, time_text);
    }
    if(status_label != NULL && status != NULL) {
        lv_label_set_text(status_label, status);
    }
    if(bar != NULL) {
        lv_bar_set_value(bar, progress, LV_ANIM_OFF);
    }
    lcd_unlock();
}
