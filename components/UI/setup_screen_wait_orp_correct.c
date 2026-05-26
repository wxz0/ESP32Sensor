#include "setup_ui.h"

lv_obj_t *screen_wait_orp_correct = NULL;
lv_obj_t *screen_wait_orp_correct_label_name = NULL;
lv_obj_t *screen_wait_orp_correct_label_time = NULL;
lv_obj_t *screen_wait_orp_correct_label_status = NULL;
lv_obj_t *screen_wait_orp_correct_bar = NULL;

static lv_obj_t *create_label(lv_obj_t *parent, int32_t y, const char *text,
                              const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_pos(label, 0, y);
    lv_obj_set_width(label, 220);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

lv_obj_t *setup_screen_wait_orp_correct(void)
{
    if(screen_wait_orp_correct != NULL) {
        return screen_wait_orp_correct;
    }

    screen_wait_orp_correct = lv_obj_create(NULL);
    lv_obj_set_scrollbar_mode(screen_wait_orp_correct, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(screen_wait_orp_correct, lv_color_hex(0xEEF3F7), 0);
    lv_obj_set_style_bg_opa(screen_wait_orp_correct, LV_OPA_COVER, 0);

    lv_obj_t *panel = lv_obj_create(screen_wait_orp_correct);
    lv_obj_set_pos(panel, 10, 30);
    lv_obj_set_size(panel, 220, 250);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0xD7E0E8), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_radius(panel, 6, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    create_label(panel, 22, "ORP CALIBRATION", &lv_font_montserrat_18, 0xEB5757);
    screen_wait_orp_correct_label_name = create_label(panel, 62, "ORP", &lv_font_montserrat_20, 0x101820);
    screen_wait_orp_correct_label_time = create_label(panel, 108, "40 s", &lv_font_montserrat_20, 0x101820);

    screen_wait_orp_correct_bar = lv_bar_create(panel);
    lv_obj_set_pos(screen_wait_orp_correct_bar, 24, 154);
    lv_obj_set_size(screen_wait_orp_correct_bar, 172, 16);
    lv_bar_set_range(screen_wait_orp_correct_bar, 0, 100);
    lv_bar_set_value(screen_wait_orp_correct_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(screen_wait_orp_correct_bar, lv_color_hex(0xDDE6EE), LV_PART_MAIN);
    lv_obj_set_style_bg_color(screen_wait_orp_correct_bar, lv_color_hex(0xEB5757), LV_PART_INDICATOR);

    screen_wait_orp_correct_label_status = create_label(panel, 194, "Stabilizing sample", &lv_font_montserrat_14, 0x52616F);

    return screen_wait_orp_correct;
}
