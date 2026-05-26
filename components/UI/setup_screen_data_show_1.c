#include "setup_ui.h"

lv_obj_t *screen_data_show_1 = NULL;
lv_obj_t *screen_data_show_1_cont_pressure1 = NULL;
lv_obj_t *screen_data_show_1_label_pressure1_text = NULL;
lv_obj_t *screen_data_show_1_label_pressure1_data = NULL;
lv_obj_t *screen_data_show_1_label_pressure1_unit = NULL;
lv_obj_t *screen_data_show_1_cont_pressure2 = NULL;
lv_obj_t *screen_data_show_1_label_pressure2_text = NULL;
lv_obj_t *screen_data_show_1_label_pressure2_data = NULL;
lv_obj_t *screen_data_show_1_label_pressure2_unit = NULL;
lv_obj_t *screen_data_show_1_cont_temp = NULL;
lv_obj_t *screen_data_show_1_label_temp_text = NULL;
lv_obj_t *screen_data_show_1_label_text_data = NULL;
lv_obj_t *screen_data_show_1_label_temp_unit = NULL;
lv_obj_t *screen_data_show_1_cont_ph = NULL;
lv_obj_t *screen_data_show_1_label_ph_text = NULL;
lv_obj_t *screen_data_show_1_label_ph_data = NULL;
lv_obj_t *screen_data_show_1_label_ph_unit = NULL;
lv_obj_t *screen_data_show_1_cont_tds = NULL;
lv_obj_t *screen_data_show_1_label_tds_text = NULL;
lv_obj_t *screen_data_show_1_label_tds_data = NULL;
lv_obj_t *screen_data_show_1_label_tds_unit = NULL;
lv_obj_t *screen_data_show_1_cont_sg = NULL;
lv_obj_t *screen_data_show_1_label_sg_text = NULL;
lv_obj_t *screen_data_show_1_label_sg_data = NULL;
lv_obj_t *screen_data_show_1_label_sg_unit = NULL;
lv_obj_t *screen_data_show_1_cont_ec = NULL;
lv_obj_t *screen_data_show_1_label_ec_text = NULL;
lv_obj_t *screen_data_show_1_label_ec_data = NULL;
lv_obj_t *screen_data_show_1_label_ec_unit = NULL;
lv_obj_t *screen_data_show_1_cont_salt1 = NULL;
lv_obj_t *screen_data_show_1_label_salt1_text = NULL;
lv_obj_t *screen_data_show_1_label_salt1_data = NULL;
lv_obj_t *screen_data_show_1_label_salt1_unit = NULL;
lv_obj_t *screen_data_show_1_cont_salt2 = NULL;
lv_obj_t *screen_data_show_1_label_salt2_text = NULL;
lv_obj_t *screen_data_show_1_label_salt2_data = NULL;
lv_obj_t *screen_data_show_1_label_salt2_unit = NULL;
lv_obj_t *screen_data_show_1_cont_orp = NULL;
lv_obj_t *screen_data_show_1_label_orp_text = NULL;
lv_obj_t *screen_data_show_1_label_orp_data = NULL;
lv_obj_t *screen_data_show_1_label_orp_unit = NULL;

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

static lv_obj_t *create_tile(lv_obj_t *parent, int32_t x, int32_t y)
{
    lv_obj_t *tile = lv_obj_create(parent);
    lv_obj_set_pos(tile, x, y);
    lv_obj_set_size(tile, 120, 64);
    lv_obj_set_layout(tile, LV_LAYOUT_NONE);
    style_tile(tile);
    return tile;
}

static lv_obj_t *create_label(lv_obj_t *parent, int32_t x, int32_t y, int32_t w,
                              const char *text, const lv_font_t *font, uint32_t color,
                              lv_text_align_t align)
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
    lv_obj_set_size(accent, 4, 64);
    lv_obj_set_style_bg_color(accent, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(accent, 0, 0);
    lv_obj_set_style_radius(accent, 0, 0);
    lv_obj_clear_flag(accent, LV_OBJ_FLAG_SCROLLABLE);
}

static void create_sensor_tile(lv_obj_t *parent, lv_obj_t **tile,
                               lv_obj_t **title, lv_obj_t **data, lv_obj_t **unit,
                               int32_t x, int32_t y, const char *title_text,
                               const char *unit_text, uint32_t accent_color)
{
    *tile = create_tile(parent, x, y);
    create_accent(*tile, accent_color);

    *title = create_label(*tile, 10, 3, 102, title_text, &lv_font_montserrat_14,
                          accent_color, LV_TEXT_ALIGN_LEFT);
    *data = create_label(*tile, 10, 22, 100, "0", &lv_font_montserrat_20,
                         0x101820, LV_TEXT_ALIGN_CENTER);

    if(unit != NULL) {
        *unit = create_label(*tile, 52, 45, 60, unit_text, &lv_font_montserrat_14,
                             0x52616F, LV_TEXT_ALIGN_RIGHT);
    }
}

lv_obj_t *setup_screen_data_show_1(void)
{
    if(screen_data_show_1 != NULL) {
        return screen_data_show_1;
    }

    screen_data_show_1 = lv_obj_create(NULL);
    style_screen(screen_data_show_1);

    create_sensor_tile(screen_data_show_1, &screen_data_show_1_cont_pressure1,
                       &screen_data_show_1_label_pressure1_text,
                       &screen_data_show_1_label_pressure1_data,
                       &screen_data_show_1_label_pressure1_unit,
                       0, 0, "PRESS1", "Pa", 0x2F80ED);

    create_sensor_tile(screen_data_show_1, &screen_data_show_1_cont_pressure2,
                       &screen_data_show_1_label_pressure2_text,
                       &screen_data_show_1_label_pressure2_data,
                       &screen_data_show_1_label_pressure2_unit,
                       120, 0, "PRESS2", "Pa", 0x2F80ED);

    create_sensor_tile(screen_data_show_1, &screen_data_show_1_cont_ph,
                       &screen_data_show_1_label_ph_text,
                       &screen_data_show_1_label_ph_data,
                       &screen_data_show_1_label_ph_unit,
                       0, 64, "PH", "pH", 0x9B51E0);

    create_sensor_tile(screen_data_show_1, &screen_data_show_1_cont_temp,
                       &screen_data_show_1_label_temp_text,
                       &screen_data_show_1_label_text_data,
                       &screen_data_show_1_label_temp_unit,
                       120, 64, "TEMP", "C", 0xF2994A);

    create_sensor_tile(screen_data_show_1, &screen_data_show_1_cont_orp,
                       &screen_data_show_1_label_orp_text,
                       &screen_data_show_1_label_orp_data,
                       &screen_data_show_1_label_orp_unit,
                       0, 128, "ORP", "mV", 0xEB5757);

    create_sensor_tile(screen_data_show_1, &screen_data_show_1_cont_tds,
                       &screen_data_show_1_label_tds_text,
                       &screen_data_show_1_label_tds_data,
                       &screen_data_show_1_label_tds_unit,
                       120, 128, "TDS", "ppt", 0x27AE60);

    create_sensor_tile(screen_data_show_1, &screen_data_show_1_cont_sg,
                       &screen_data_show_1_label_sg_text,
                       &screen_data_show_1_label_sg_data,
                       &screen_data_show_1_label_sg_unit,
                       0, 192, "SG", "SG", 0x56CCF2);

    create_sensor_tile(screen_data_show_1, &screen_data_show_1_cont_ec,
                       &screen_data_show_1_label_ec_text,
                       &screen_data_show_1_label_ec_data,
                       &screen_data_show_1_label_ec_unit,
                       120, 192, "EC", "ms/cm", 0x00A8A8);

    create_sensor_tile(screen_data_show_1, &screen_data_show_1_cont_salt1,
                       &screen_data_show_1_label_salt1_text,
                       &screen_data_show_1_label_salt1_data,
                       &screen_data_show_1_label_salt1_unit,
                       0, 256, "SALT1", "ppt", 0xB8860B);

    create_sensor_tile(screen_data_show_1, &screen_data_show_1_cont_salt2,
                       &screen_data_show_1_label_salt2_text,
                       &screen_data_show_1_label_salt2_data,
                       &screen_data_show_1_label_salt2_unit,
                       120, 256, "SALT2", "%", 0xB8860B);

    return screen_data_show_1;
}
