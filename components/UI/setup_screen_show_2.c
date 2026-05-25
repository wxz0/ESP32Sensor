#include "setup_ui.h"

lv_obj_t * screen_show_2 = NULL;
lv_obj_t * screen_show_2_cont_lux1 = NULL;
lv_obj_t * screen_show_2_label_lux1_text = NULL;
lv_obj_t * screen_show_2_label_lux1_data = NULL;
lv_obj_t * screen_show_2_label_lux1_unit = NULL;
lv_obj_t * screen_show_2_cont_lux2 = NULL;
lv_obj_t * screen_show_2_label_lux2_text = NULL;
lv_obj_t * screen_show_2_label_lux2_data = NULL;
lv_obj_t * screen_show_2_label_lux2_unit = NULL;
lv_obj_t * screen_show_2_cont_lux3 = NULL;
lv_obj_t * screen_show_2_label_lux3_text = NULL;
lv_obj_t * screen_show_2_label_lux3_data = NULL;
lv_obj_t * screen_show_2_label_lux3_unit = NULL;
lv_obj_t * screen_show_2_cont_lux4 = NULL;
lv_obj_t * screen_show_2_label_lux4_text = NULL;
lv_obj_t * screen_show_2_label_lux_data = NULL;
lv_obj_t * screen_show_2_label_lux_unit = NULL;

static lv_obj_t * create_ui(void);

static void add_tile_style(lv_obj_t *cont) {
    lv_obj_set_style_bg_color(cont, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(cont, lv_color_hex(0xFE0101), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(cont, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
}

static lv_obj_t * create_tile(lv_obj_t *parent, int x, int y) {
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_x(cont, x);
    lv_obj_set_y(cont, y);
    lv_obj_set_width(cont, 120);
    lv_obj_set_height(cont, 64);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_layout(cont, LV_LAYOUT_NONE);
    add_tile_style(cont);
    return cont;
}

static lv_obj_t * create_label(lv_obj_t *parent, int x, int y, const char *text,
                                const lv_font_t *font, uint32_t color) {
    lv_obj_t *lbl = lv_label_create(parent);
    lv_obj_set_x(lbl, x);
    lv_obj_set_y(lbl, y);
    lv_obj_set_width(lbl, LV_SIZE_CONTENT);
    lv_obj_set_height(lbl, LV_SIZE_CONTENT);
    lv_obj_set_scrollbar_mode(lbl, LV_SCROLLBAR_MODE_OFF);
    lv_label_set_text(lbl, text);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    if (color != 0) {
        lv_obj_set_style_text_color(lbl, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    lv_obj_set_style_text_font(lbl, font, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    return lbl;
}

static lv_obj_t * create_ui(void) {
    screen_show_2 = lv_obj_create(NULL);
    lv_obj_set_scrollbar_mode(screen_show_2, LV_SCROLLBAR_MODE_OFF);

    /* Row 1: Lux1, Lux2 */
    screen_show_2_cont_lux1 = create_tile(screen_show_2, 0, 0);
    screen_show_2_label_lux1_text = create_label(screen_show_2_cont_lux1, 7, -1, "\xe5\x85\x89\xe5\xbc\xba\x31", &lv_front_song_Chinese_18, 0xFB0197);
    screen_show_2_label_lux1_data = create_label(screen_show_2_cont_lux1, 7, 23, "0", &lv_font_montserrat_18, 0);
    screen_show_2_label_lux1_unit = create_label(screen_show_2_cont_lux1, 34, 42, "Lux", &lv_font_montserrat_14, 0);

    screen_show_2_cont_lux2 = create_tile(screen_show_2, 120, 0);
    screen_show_2_label_lux2_text = create_label(screen_show_2_cont_lux2, 6, 0, "\xe5\x85\x89\xe5\xbc\xba\x32", &lv_front_song_Chinese_18, 0xFB0197);
    screen_show_2_label_lux2_data = create_label(screen_show_2_cont_lux2, 6, 24, "0", &lv_font_montserrat_18, 0);
    screen_show_2_label_lux2_unit = create_label(screen_show_2_cont_lux2, 35, 42, "Lux", &lv_font_montserrat_14, 0);

    /* Row 2: Lux3, Lux4 */
    screen_show_2_cont_lux3 = create_tile(screen_show_2, 0, 64);
    screen_show_2_label_lux3_text = create_label(screen_show_2_cont_lux3, 7, -1, "\xe5\x85\x89\xe5\xbc\xba\x33", &lv_front_song_Chinese_18, 0xFB0197);
    screen_show_2_label_lux3_data = create_label(screen_show_2_cont_lux3, 6, 22, "0", &lv_font_montserrat_18, 0);
    screen_show_2_label_lux3_unit = create_label(screen_show_2_cont_lux3, 35, 42, "Lux", &lv_font_montserrat_14, 0);

    screen_show_2_cont_lux4 = create_tile(screen_show_2, 120, 64);
    screen_show_2_label_lux4_text = create_label(screen_show_2_cont_lux4, 8, 0, "\xe5\x85\x89\xe5\xbc\xba\x34", &lv_front_song_Chinese_18, 0xFB0197);
    screen_show_2_label_lux_data = create_label(screen_show_2_cont_lux4, 8, 24, "0", &lv_font_montserrat_18, 0);
    screen_show_2_label_lux_unit = create_label(screen_show_2_cont_lux4, 35, 42, "Lux", &lv_font_montserrat_14, 0);

    return screen_show_2;
}

lv_obj_t * setup_screen_show_2(void) {
    if (screen_show_2 != NULL) {
        return screen_show_2;
    }
    create_ui();
    return screen_show_2;
}
