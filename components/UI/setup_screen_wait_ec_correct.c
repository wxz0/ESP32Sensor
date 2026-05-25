#include "setup_ui.h"

lv_obj_t * screen_wait_ec_correct = NULL;
lv_obj_t * screen_wait_ec_correct_spinner = NULL;
lv_obj_t * screen_wait_ec_correct_label = NULL;

static lv_obj_t * create_ui(void) {
    screen_wait_ec_correct = lv_obj_create(NULL);
    lv_obj_set_scrollbar_mode(screen_wait_ec_correct, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *cont = lv_obj_create(screen_wait_ec_correct);
    lv_obj_set_x(cont, 0);
    lv_obj_set_y(cont, 0);
    lv_obj_set_width(cont, 240);
    lv_obj_set_height(cont, 320);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_layout(cont, LV_LAYOUT_NONE);
    lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    screen_wait_ec_correct_spinner = lv_spinner_create(cont);
    lv_obj_set_x(screen_wait_ec_correct_spinner, 70);
    lv_obj_set_y(screen_wait_ec_correct_spinner, 60);
    lv_obj_set_width(screen_wait_ec_correct_spinner, 100);
    lv_obj_set_height(screen_wait_ec_correct_spinner, 100);
    lv_spinner_set_anim_params(screen_wait_ec_correct_spinner, 10000, 60);
    lv_obj_set_style_arc_color(screen_wait_ec_correct_spinner, lv_color_hex(0xF32147), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(screen_wait_ec_correct_spinner, 15, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_rounded(screen_wait_ec_correct_spinner, true, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    screen_wait_ec_correct_label = lv_label_create(cont);
    lv_obj_set_x(screen_wait_ec_correct_label, 4);
    lv_obj_set_y(screen_wait_ec_correct_label, 218);
    lv_obj_set_width(screen_wait_ec_correct_label, LV_SIZE_CONTENT);
    lv_obj_set_height(screen_wait_ec_correct_label, LV_SIZE_CONTENT);
    lv_obj_set_scrollbar_mode(screen_wait_ec_correct_label, LV_SCROLLBAR_MODE_OFF);
    lv_label_set_text(screen_wait_ec_correct_label, "\xe6\xad\xa3\xe5\x9c\xa8\xe8\xbf\x9b\xe8\xa1\x8c""EC\xe6\xa0\xa1\xe5\x87\x86\xef\xbc\x8c\xe8\xaf\xb7\xe8\x80\x90\xe5\xbf\x83\xe7\xad\x89\xe5\xbe\x85");
    lv_label_set_long_mode(screen_wait_ec_correct_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(screen_wait_ec_correct_label, &lv_front_song_Chinese_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(screen_wait_ec_correct_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

    return screen_wait_ec_correct;
}

lv_obj_t * setup_screen_wait_ec_correct(void) {
    if (screen_wait_ec_correct != NULL) {
        return screen_wait_ec_correct;
    }
    create_ui();
    return screen_wait_ec_correct;
}