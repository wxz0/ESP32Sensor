#include "setup_ui.h"

lv_obj_t *screen_show_2 = NULL;
lv_obj_t *screen_show_2_cont_lux1 = NULL;
lv_obj_t *screen_show_2_label_lux1_text = NULL;
lv_obj_t *screen_show_2_label_lux1_data = NULL;
lv_obj_t *screen_show_2_label_lux1_unit = NULL;
lv_obj_t *screen_show_2_cont_lux2 = NULL;
lv_obj_t *screen_show_2_label_lux2_text = NULL;
lv_obj_t *screen_show_2_label_lux2_data = NULL;
lv_obj_t *screen_show_2_label_lux2_unit = NULL;
lv_obj_t *screen_show_2_cont_lux3 = NULL;
lv_obj_t *screen_show_2_label_lux3_text = NULL;
lv_obj_t *screen_show_2_label_lux3_data = NULL;
lv_obj_t *screen_show_2_label_lux3_unit = NULL;
lv_obj_t *screen_show_2_cont_lux4 = NULL;
lv_obj_t *screen_show_2_label_lux4_text = NULL;
lv_obj_t *screen_show_2_label_lux_data = NULL;
lv_obj_t *screen_show_2_label_lux_unit = NULL;

static void style_screen(lv_obj_t *obj)
{
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xEEF3F7), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

static void style_tile(lv_obj_t *obj)
{
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(0xD7E0E8), 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_radius(obj, 3, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

static lv_obj_t *create_label(lv_obj_t *parent, int32_t x, int32_t y, int32_t w,
                              const char *text, const lv_font_t *font,
                              uint32_t color, lv_text_align_t align)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, w, LV_SIZE_CONTENT);
    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(label, align, 0);
    return label;
}

static void create_accent(lv_obj_t *parent, uint32_t color)
{
    lv_obj_t *accent = lv_obj_create(parent);
    lv_obj_set_pos(accent, 0, 0);
    lv_obj_set_size(accent, 5, 160);
    lv_obj_set_style_bg_color(accent, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(accent, 0, 0);
    lv_obj_set_style_radius(accent, 0, 0);
    lv_obj_clear_flag(accent, LV_OBJ_FLAG_SCROLLABLE);
}

static void create_lux_tile(lv_obj_t *parent, lv_obj_t **tile, lv_obj_t **title,
                            lv_obj_t **data, lv_obj_t **unit, int32_t x,
                            int32_t y, const char *title_text, uint32_t accent_color)
{
    *tile = lv_obj_create(parent);
    lv_obj_set_pos(*tile, x, y);
    lv_obj_set_size(*tile, 120, 160);
    lv_obj_set_layout(*tile, LV_LAYOUT_NONE);
    style_tile(*tile);
    create_accent(*tile, accent_color);

    *title = create_label(*tile, 12, 10, 98, title_text, &lv_font_montserrat_18,
                          accent_color, LV_TEXT_ALIGN_LEFT);
    *data = create_label(*tile, 10, 58, 100, "0", &lv_font_montserrat_20,
                         0x101820, LV_TEXT_ALIGN_CENTER);
    *unit = create_label(*tile, 54, 126, 58, "Lux", &lv_font_montserrat_18,
                         0x52616F, LV_TEXT_ALIGN_RIGHT);
}

lv_obj_t *setup_screen_show_2(void)
{
    if(screen_show_2 != NULL) {
        return screen_show_2;
    }

    screen_show_2 = lv_obj_create(NULL);
    style_screen(screen_show_2);

    create_lux_tile(screen_show_2, &screen_show_2_cont_lux1,
                    &screen_show_2_label_lux1_text,
                    &screen_show_2_label_lux1_data,
                    &screen_show_2_label_lux1_unit,
                    0, 0, "LUX1", 0x2F80ED);

    create_lux_tile(screen_show_2, &screen_show_2_cont_lux2,
                    &screen_show_2_label_lux2_text,
                    &screen_show_2_label_lux2_data,
                    &screen_show_2_label_lux2_unit,
                    120, 0, "LUX2", 0x00A8A8);

    create_lux_tile(screen_show_2, &screen_show_2_cont_lux3,
                    &screen_show_2_label_lux3_text,
                    &screen_show_2_label_lux3_data,
                    &screen_show_2_label_lux3_unit,
                    0, 160, "LUX3", 0xF2994A);

    create_lux_tile(screen_show_2, &screen_show_2_cont_lux4,
                    &screen_show_2_label_lux4_text,
                    &screen_show_2_label_lux_data,
                    &screen_show_2_label_lux_unit,
                    120, 160, "LUX4", 0x9B51E0);

    return screen_show_2;
}
