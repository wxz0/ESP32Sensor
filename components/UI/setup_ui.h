#ifndef __SETUP_UI_H__
#define __SETUP_UI_H__

#include "lvgl.h"

LV_FONT_DECLARE(lv_front_song_Chinese_18);

void setupUi(void);

lv_obj_t * setup_screen_data_show_1(void);
lv_obj_t * setup_screen_show_2(void);
lv_obj_t * setup_screen_wait_ec_correct(void);
lv_obj_t * setup_screen_wait_ph_correct(void);
lv_obj_t * setup_screen_wait_orp_correct(void);

#endif