#ifndef SETUP_UI_H
#define SETUP_UI_H

#include "lvgl.h"

void setupUi(void);

lv_obj_t *setup_screen_data_show_1(void);
lv_obj_t *setup_screen_show_2(void);
lv_obj_t *setup_screen_wait_ec_correct(void);
lv_obj_t *setup_screen_wait_ph_correct(void);
lv_obj_t *setup_screen_wait_orp_correct(void);

#endif
