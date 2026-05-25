#include "setup_ui.h"

lv_obj_t * screen_data_show_1 = NULL;
lv_obj_t * screen_data_show_1_cont_pressure1 = NULL;
lv_obj_t * screen_data_show_1_label_pressure1_text = NULL;
lv_obj_t * screen_data_show_1_label_pressure1_data = NULL;
lv_obj_t * screen_data_show_1_label_pressure1_unit = NULL;
lv_obj_t * screen_data_show_1_cont_pressure2 = NULL;
lv_obj_t * screen_data_show_1_label_pressure2_text = NULL;
lv_obj_t * screen_data_show_1_label_pressure2_data = NULL;
lv_obj_t * screen_data_show_1_label_pressure2_unit = NULL;
lv_obj_t * screen_data_show_1_cont_temp = NULL;
lv_obj_t * screen_data_show_1_label_temp_text = NULL;
lv_obj_t * screen_data_show_1_label_text_data = NULL;
lv_obj_t * screen_data_show_1_label_temp_unit = NULL;
lv_obj_t * screen_data_show_1_cont_ph = NULL;
lv_obj_t * screen_data_show_1_label_ph_text = NULL;
lv_obj_t * screen_data_show_1_label_ph_data = NULL;
lv_obj_t * screen_data_show_1_cont_tds = NULL;
lv_obj_t * screen_data_show_1_label_tds_text = NULL;
lv_obj_t * screen_data_show_1_label_tds_data = NULL;
lv_obj_t * screen_data_show_1_label_tds_unit = NULL;
lv_obj_t * screen_data_show_1_cont_sg = NULL;
lv_obj_t * screen_data_show_1_label_sg_text = NULL;
lv_obj_t * screen_data_show_1_label_sg_data = NULL;
lv_obj_t * screen_data_show_1_cont_ec = NULL;
lv_obj_t * screen_data_show_1_label_ec_text = NULL;
lv_obj_t * screen_data_show_1_label_ec_data = NULL;
lv_obj_t * screen_data_show_1_label_ec_unit = NULL;
lv_obj_t * screen_data_show_1_cont_salt1 = NULL;
lv_obj_t * screen_data_show_1_label_salt1_text = NULL;
lv_obj_t * screen_data_show_1_label_salt1_data = NULL;
lv_obj_t * screen_data_show_1_label_salt1_unit = NULL;
lv_obj_t * screen_data_show_1_cont_salt2 = NULL;
lv_obj_t * screen_data_show_1_label_salt2_text = NULL;
lv_obj_t * screen_data_show_1_label_salt2_data = NULL;
lv_obj_t * screen_data_show_1_label_salt2_unit = NULL;
lv_obj_t * screen_data_show_1_cont_orp = NULL;
lv_obj_t * screen_data_show_1_label_orp_text = NULL;
lv_obj_t * screen_data_show_1_label_orp_data = NULL;
lv_obj_t * screen_data_show_1_label_orp_unit = NULL;

static lv_obj_t * create_ui(void);

static void add_tile_style(lv_obj_t *cont, uint32_t border_color) {
    lv_obj_set_style_bg_color(cont, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(cont, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(cont, lv_color_hex(border_color), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(cont, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(cont, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
}

static lv_obj_t * create_tile(lv_obj_t *parent, int x, int y, uint32_t border_color) {
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_x(cont, x);
    lv_obj_set_y(cont, y);
    lv_obj_set_width(cont, 120);
    lv_obj_set_height(cont, 64);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_layout(cont, LV_LAYOUT_NONE);
    add_tile_style(cont, border_color);
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
    screen_data_show_1 = lv_obj_create(NULL);
    lv_obj_set_scrollbar_mode(screen_data_show_1, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(screen_data_show_1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(screen_data_show_1, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* Row 1: Pressure1, Pressure2 */
    screen_data_show_1_cont_pressure1 = create_tile(screen_data_show_1, 0, 0, 0xFB0F0F);
    screen_data_show_1_label_pressure1_text = create_label(screen_data_show_1_cont_pressure1, 7, -1, "\xe5\x8e\x8b\xe5\xbc\xba\x31", &lv_front_song_Chinese_18, 0xFB0197);
    screen_data_show_1_label_pressure1_data = create_label(screen_data_show_1_cont_pressure1, 7, 22, "0", &lv_font_montserrat_18, 0);
    screen_data_show_1_label_pressure1_unit = create_label(screen_data_show_1_cont_pressure1, 42, 42, "Pa", &lv_font_montserrat_14, 0);

    screen_data_show_1_cont_pressure2 = create_tile(screen_data_show_1, 120, 0, 0xFB0A0A);
    screen_data_show_1_label_pressure2_text = create_label(screen_data_show_1_cont_pressure2, 6, 0, "\xe5\x8e\x8b\xe5\xbc\xba\x32", &lv_front_song_Chinese_18, 0xFB0197);
    screen_data_show_1_label_pressure2_data = create_label(screen_data_show_1_cont_pressure2, 6, 24, "0", &lv_font_montserrat_18, 0);
    screen_data_show_1_label_pressure2_unit = create_label(screen_data_show_1_cont_pressure2, 42, 42, "Pa", &lv_font_montserrat_14, 0);

    /* Row 2: PH, Temperature */
    screen_data_show_1_cont_ph = create_tile(screen_data_show_1, 0, 64, 0xFF0707);
    screen_data_show_1_label_ph_text = create_label(screen_data_show_1_cont_ph, 7, -1, "PH", &lv_font_montserrat_18, 0xFB0197);
    screen_data_show_1_label_ph_data = create_label(screen_data_show_1_cont_ph, 7, 24, "0", &lv_font_montserrat_18, 0);

    screen_data_show_1_cont_temp = create_tile(screen_data_show_1, 120, 64, 0xFF0101);
    screen_data_show_1_label_temp_text = create_label(screen_data_show_1_cont_temp, 8, 0, "\xe6\xb8\xa9\xe5\xba\xa6", &lv_front_song_Chinese_18, 0xFB0197);
    screen_data_show_1_label_text_data = create_label(screen_data_show_1_cont_temp, 8, 24, "0", &lv_font_montserrat_18, 0);
    screen_data_show_1_label_temp_unit = create_label(screen_data_show_1_cont_temp, 42, 42, "\xc2\xb0" "C", &lv_font_montserrat_14, 0);

    /* Row 3: ORP, TDS */
    screen_data_show_1_cont_orp = create_tile(screen_data_show_1, 0, 128, 0xFE0101);
    screen_data_show_1_label_orp_text = create_label(screen_data_show_1_cont_orp, 7, 0, "ORP", &lv_font_montserrat_18, 0xFB0197);
    screen_data_show_1_label_orp_data = create_label(screen_data_show_1_cont_orp, 7, 24, "0", &lv_font_montserrat_18, 0);
    screen_data_show_1_label_orp_unit = create_label(screen_data_show_1_cont_orp, 37, 42, "mV", &lv_font_montserrat_14, 0);

    screen_data_show_1_cont_tds = create_tile(screen_data_show_1, 120, 128, 0xFB0101);
    screen_data_show_1_label_tds_text = create_label(screen_data_show_1_cont_tds, 9, 0, "TDS", &lv_font_montserrat_18, 0xFB0197);
    screen_data_show_1_label_tds_data = create_label(screen_data_show_1_cont_tds, 9, 24, "0", &lv_font_montserrat_18, 0);
    screen_data_show_1_label_tds_unit = create_label(screen_data_show_1_cont_tds, 35, 42, "ppt", &lv_font_montserrat_14, 0);

    /* Row 4: SG, EC */
    screen_data_show_1_cont_sg = create_tile(screen_data_show_1, 0, 192, 0xFE0101);
    screen_data_show_1_label_sg_text = create_label(screen_data_show_1_cont_sg, 10, 0, "SG", &lv_font_montserrat_18, 0xFB0197);
    screen_data_show_1_label_sg_data = create_label(screen_data_show_1_cont_sg, 10, 24, "0", &lv_font_montserrat_18, 0);

    screen_data_show_1_cont_ec = create_tile(screen_data_show_1, 120, 192, 0xFF0505);
    screen_data_show_1_label_ec_text = create_label(screen_data_show_1_cont_ec, 13, 0, "EC", &lv_font_montserrat_18, 0xFB0197);
    screen_data_show_1_label_ec_data = create_label(screen_data_show_1_cont_ec, 13, 24, "0", &lv_font_montserrat_18, 0);
    screen_data_show_1_label_ec_unit = create_label(screen_data_show_1_cont_ec, 10, 42, "ms/cm", &lv_font_montserrat_14, 0);

    /* Row 5: SALT1, SALT2 */
    screen_data_show_1_cont_salt1 = create_tile(screen_data_show_1, 0, 256, 0xFF0101);
    screen_data_show_1_label_salt1_text = create_label(screen_data_show_1_cont_salt1, 7, 0, "SALT1", &lv_font_montserrat_18, 0xFB0197);
    screen_data_show_1_label_salt1_data = create_label(screen_data_show_1_cont_salt1, 7, 22, "0", &lv_font_montserrat_18, 0);
    screen_data_show_1_label_salt1_unit = create_label(screen_data_show_1_cont_salt1, 35, 42, "ppt", &lv_font_montserrat_14, 0);

    screen_data_show_1_cont_salt2 = create_tile(screen_data_show_1, 120, 256, 0xFB0808);
    screen_data_show_1_label_salt2_text = create_label(screen_data_show_1_cont_salt2, 3, -1, "SALT2", &lv_font_montserrat_18, 0xFB0197);
    screen_data_show_1_label_salt2_data = create_label(screen_data_show_1_cont_salt2, 11, 21, "0", &lv_font_montserrat_18, 0);
    screen_data_show_1_label_salt2_unit = create_label(screen_data_show_1_cont_salt2, 46, 42, "%", &lv_font_montserrat_14, 0);

    return screen_data_show_1;
}

lv_obj_t * setup_screen_data_show_1(void) {
    if (screen_data_show_1 != NULL) {
        return screen_data_show_1;
    }
    create_ui();
    return screen_data_show_1;
}
