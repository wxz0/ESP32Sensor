idf_component_register(
    SRCS
        "setup_ui.c"
        "setup_screen_data_show_1.c"
        "setup_screen_show_2.c"
        "setup_screen_wait_ec_correct.c"
        "setup_screen_wait_ph_correct.c"
        "setup_screen_wait_orp_correct.c"
        "animations.c"
        "fonts/lv_front_song_Chinese_18.c"
    INCLUDE_DIRS "."
    REQUIRES lvgl__lvgl
)
