idf_component_register(
    SRCS "Usb_Storage.c"
    INCLUDE_DIRS "include"
    REQUIRES
        Common
        esp_timer
        fatfs
        usb
        usb_host_msc
        vfs
)
