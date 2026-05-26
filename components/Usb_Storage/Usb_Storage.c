#include "Usb_Storage.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "usb/msc_host.h"
#include "usb/msc_host_vfs.h"
#include "usb/usb_host.h"

static const char *TAG = "Usb_Storage";

#define USB_MOUNT_PATH "/usb"
#define USB_DATA_DIR USB_MOUNT_PATH "/SensorData"
#define USB_LOG_QUEUE_LENGTH 32
#define USB_LOG_LINE_MAX 192

typedef struct {
    char line[USB_LOG_LINE_MAX];
} usb_log_msg_t;

typedef struct {
    enum {
        USB_APP_DEVICE_CONNECTED,
        USB_APP_DEVICE_DISCONNECTED,
    } id;
    union {
        uint8_t new_dev_address;
        msc_host_device_handle_t device_handle;
    } data;
} usb_app_msg_t;

static QueueHandle_t s_usb_app_queue = NULL;
static QueueHandle_t s_log_queue = NULL;
static msc_host_device_handle_t s_msc_device = NULL;
static msc_host_vfs_handle_t s_vfs_handle = NULL;
static volatile bool s_mounted = false;
static volatile bool s_usb_started = false;

static bool get_local_time(struct tm *out_tm)
{
    time_t now = 0;

    if(out_tm == NULL) {
        return false;
    }

    time(&now);
    localtime_r(&now, out_tm);
    return (out_tm->tm_year + 1900) >= 2024;
}

static void make_time_prefix(char *buf, size_t len)
{
    struct tm tm_now = {0};

    if(buf == NULL || len == 0) {
        return;
    }

    if(get_local_time(&tm_now)) {
        strftime(buf, len, "%Y-%m-%d %H:%M:%S", &tm_now);
    } else {
        snprintf(buf, len, "uptime=%llds", (long long)(esp_timer_get_time() / 1000000LL));
    }
}

static void make_log_path(char *buf, size_t len)
{
    struct tm tm_now = {0};

    if(get_local_time(&tm_now)) {
        char date_name[16];
        strftime(date_name, sizeof(date_name), "%Y%m%d", &tm_now);
        snprintf(buf, len, USB_DATA_DIR "/%s.txt", date_name);
    } else {
        snprintf(buf, len, USB_DATA_DIR "/unsynced.txt");
    }
}

static void submit_line(const char *line)
{
    usb_log_msg_t msg = {0};

    if(s_log_queue == NULL || line == NULL) {
        return;
    }

    strlcpy(msg.line, line, sizeof(msg.line));
    if(xQueueSend(s_log_queue, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "USB log queue full, drop new log");
    }
}

void Usb_Storage_LogWater(const WaterQualityData_t *data)
{
    char ts[32];
    char line[USB_LOG_LINE_MAX];

    if(data == NULL) {
        return;
    }

    make_time_prefix(ts, sizeof(ts));
    snprintf(line, sizeof(line),
             "%s WATER temp=%.2fC ph=%.2f tds=%lu orp=%d mV sg=%.3f ec=%lu ms/cm salt_ppt=%.1f salt_pct=%.1f cl=%.2f mg/L",
             ts,
             data->temp,
             data->ph,
             (unsigned long)data->tds,
             (int)data->orp,
             data->sg,
             (unsigned long)data->ec,
             data->saltppt,
             data->salt,
             data->cl);
    submit_line(line);
}

void Usb_Storage_LogLight(const LightSensorData_t *data)
{
    char ts[32];
    char line[USB_LOG_LINE_MAX];

    if(data == NULL) {
        return;
    }

    make_time_prefix(ts, sizeof(ts));
    snprintf(line, sizeof(line),
             "%s LIGHT lux1=%.1f Lux lux2=%.1f Lux lux3=%.1f Lux lux4=%.1f Lux",
             ts,
             data->lux_1,
             data->lux_2,
             data->lux_3,
             data->lux_4);
    submit_line(line);
}

void Usb_Storage_LogPressure(uint8_t sensor_index, const PressureSensorData_t *data)
{
    char ts[32];
    char line[USB_LOG_LINE_MAX];

    if(data == NULL) {
        return;
    }

    make_time_prefix(ts, sizeof(ts));
    snprintf(line, sizeof(line),
             "%s PRESSURE%u pressure=%.2f Pa min=%.2f Pa max=%.2f Pa",
             ts,
             (unsigned)(sensor_index + 1U),
             data->pressure_cur,
             data->pressure_min,
             data->pressure_max);
    submit_line(line);
}

static void msc_event_cb(const msc_host_event_t *event, void *arg)
{
    (void)arg;

    if(s_usb_app_queue == NULL || event == NULL) {
        return;
    }

    usb_app_msg_t msg = {0};
    if(event->event == MSC_DEVICE_CONNECTED) {
        msg.id = USB_APP_DEVICE_CONNECTED;
        msg.data.new_dev_address = event->device.address;
        xQueueSend(s_usb_app_queue, &msg, 0);
    } else if(event->event == MSC_DEVICE_DISCONNECTED) {
        msg.id = USB_APP_DEVICE_DISCONNECTED;
        msg.data.device_handle = event->device.handle;
        xQueueSend(s_usb_app_queue, &msg, 0);
    }
}

static esp_err_t mount_device(uint8_t address)
{
    const esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 2,
        .allocation_unit_size = 8192,
    };
    esp_err_t err;

    if(s_mounted) {
        ESP_LOGW(TAG, "USB disk already mounted, ignore new device");
        return ESP_ERR_INVALID_STATE;
    }

    err = msc_host_install_device(address, &s_msc_device);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "msc_host_install_device failed: %s", esp_err_to_name(err));
        s_msc_device = NULL;
        return err;
    }

    err = msc_host_vfs_register(s_msc_device, USB_MOUNT_PATH, &mount_config, &s_vfs_handle);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "msc_host_vfs_register failed: %s", esp_err_to_name(err));
        msc_host_uninstall_device(s_msc_device);
        s_msc_device = NULL;
        s_vfs_handle = NULL;
        return err;
    }

    s_mounted = true;
    ESP_LOGI(TAG, "USB disk mounted at %s", USB_MOUNT_PATH);
    return ESP_OK;
}

static void unmount_device(msc_host_device_handle_t handle)
{
    if(s_msc_device != handle) {
        ESP_LOGW(TAG, "Disconnected USB disk handle does not match mounted device");
        return;
    }

    s_mounted = false;
    if(s_vfs_handle != NULL) {
        msc_host_vfs_unregister(s_vfs_handle);
        s_vfs_handle = NULL;
    }
    if(s_msc_device != NULL) {
        msc_host_uninstall_device(s_msc_device);
        s_msc_device = NULL;
    }
    ESP_LOGI(TAG, "USB disk unmounted");
}

static bool ensure_data_dir(void)
{
    struct stat st = {0};

    if(stat(USB_DATA_DIR, &st) == 0) {
        return true;
    }

    if(mkdir(USB_DATA_DIR, 0775) != 0) {
        ESP_LOGE(TAG, "mkdir %s failed: errno=%d", USB_DATA_DIR, errno);
        return false;
    }

    return true;
}

static void write_log_line(const char *line)
{
    char path[64];
    FILE *f;

    if(!s_mounted) {
        ESP_LOGW(TAG, "USB disk not mounted");
        return;
    }

    if(!ensure_data_dir()) {
        return;
    }

    make_log_path(path, sizeof(path));
    f = fopen(path, "a");
    if(f == NULL) {
        ESP_LOGE(TAG, "open %s failed: errno=%d", path, errno);
        return;
    }

    fprintf(f, "%s\n", line);
    fflush(f);
    fsync(fileno(f));
    fclose(f);
}

static void usb_host_task(void *arg)
{
    (void)arg;

    const usb_host_config_t host_config = {
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    const msc_host_driver_config_t msc_config = {
        .create_backround_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .callback = msc_event_cb,
    };
    ESP_ERROR_CHECK(msc_host_install(&msc_config));

    while(1) {
        uint32_t event_flags = 0;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if(event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            usb_host_device_free_all();
        }
    }
}

static void usb_storage_task(void *arg)
{
    (void)arg;

    while(1) {
        usb_app_msg_t app_msg;
        usb_log_msg_t log_msg;

        while(s_usb_app_queue != NULL && xQueueReceive(s_usb_app_queue, &app_msg, 0) == pdTRUE) {
            if(app_msg.id == USB_APP_DEVICE_CONNECTED) {
                mount_device(app_msg.data.new_dev_address);
            } else if(app_msg.id == USB_APP_DEVICE_DISCONNECTED) {
                unmount_device(app_msg.data.device_handle);
            }
        }

        if(s_log_queue != NULL && xQueueReceive(s_log_queue, &log_msg, pdMS_TO_TICKS(100)) == pdTRUE) {
            write_log_line(log_msg.line);
        }
    }
}

void Usb_Storage_Init(void)
{
    if(s_usb_started) {
        return;
    }

    s_usb_app_queue = xQueueCreate(8, sizeof(usb_app_msg_t));
    s_log_queue = xQueueCreate(USB_LOG_QUEUE_LENGTH, sizeof(usb_log_msg_t));
    if(s_usb_app_queue == NULL || s_log_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create USB storage queues");
        return;
    }

    if(xTaskCreatePinnedToCore(usb_host_task, "usb_host", 4096, NULL, 4, NULL, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create USB host task");
        return;
    }
    if(xTaskCreatePinnedToCore(usb_storage_task, "usb_storage", 4096, NULL, 4, NULL, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create USB storage task");
        return;
    }

    s_usb_started = true;
    ESP_LOGI(TAG, "USB storage logger initialized");
}
