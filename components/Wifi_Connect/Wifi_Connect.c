#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "Wifi_Connect.h"
#include "esp_bus.h"
#include "esp_err.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "esp_wifi_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "Wifi_Connect";

static bool s_wifi_manager_started = false;
static uint8_t s_wifi_connected = 0;
static TaskHandle_t s_wifi_reconnect_task_handle = NULL;

static void wifi_reconnect_task(void *arg)
{
    (void)arg;

    while (1) {
        if (s_wifi_manager_started && !Wifi_IS_Connected()) {
            ESP_LOGI(TAG, "WiFi disconnected, retry saved networks while AP portal is active");
            esp_err_t ret = wifi_manager_connect(NULL);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "WiFi reconnect request failed: %s", esp_err_to_name(ret));
            }
        }

        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(15000));
    }
}

static esp_err_t wifi_littlefs_init(void)
{
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/littlefs",
        .partition_label = "storage",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LittleFS mount failed: %s", esp_err_to_name(ret));
        return ret;
    }

    size_t total = 0;
    size_t used = 0;
    ret = esp_littlefs_info("storage", &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "LittleFS mounted: %u/%u bytes used",
                 (unsigned int)used, (unsigned int)total);
    }

    return ESP_OK;
}

static void wifi_manager_event_handler(const char *event, const void *data, size_t len, void *ctx)
{
    (void)len;
    (void)ctx;

    if (strcmp(event, WIFI_EVT(WIFI_MGR_EVT_CONNECTED)) == 0) {
        const wifi_connected_t *info = (const wifi_connected_t *)data;
        s_wifi_connected = 1;
        ESP_LOGI(TAG, "WiFi connected: ssid=%s rssi=%d channel=%u",
                 info->ssid, info->rssi, info->channel);
    } else if (strcmp(event, WIFI_EVT(WIFI_MGR_EVT_DISCONNECTED)) == 0) {
        const wifi_disconnected_t *info = (const wifi_disconnected_t *)data;
        s_wifi_connected = 0;
        ESP_LOGW(TAG, "WiFi disconnected: ssid=%s reason=%u", info->ssid, info->reason);
        if (s_wifi_reconnect_task_handle != NULL) {
            xTaskNotifyGive(s_wifi_reconnect_task_handle);
        }
    } else if (strcmp(event, WIFI_EVT(WIFI_MGR_EVT_GOT_IP)) == 0) {
        wifi_status_t status;
        if (wifi_manager_get_status(&status) == ESP_OK) {
            s_wifi_connected = 1;
            ESP_LOGI(TAG, "WiFi got IP: %s, web UI: http://%s/", status.ip, status.ip);
        }
    }
}

void Wifi_Init(void)
{
    if (s_wifi_manager_started) {
        ESP_LOGW(TAG, "WiFi manager already started");
        return;
    }

    esp_err_t ret = wifi_littlefs_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Custom WebUI unavailable, WiFi Manager will use embedded UI if enabled");
    }

    ret = esp_bus_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_bus init failed: %s", esp_err_to_name(ret));
        return;
    }

    esp_bus_sub(WIFI_EVT(WIFI_MGR_EVT_CONNECTED), wifi_manager_event_handler, NULL);
    esp_bus_sub(WIFI_EVT(WIFI_MGR_EVT_DISCONNECTED), wifi_manager_event_handler, NULL);
    esp_bus_sub(WIFI_EVT(WIFI_MGR_EVT_GOT_IP), wifi_manager_event_handler, NULL);

    wifi_manager_config_t config = {
        .max_retry_per_network = 3,
        .retry_interval_ms = 5000,
        .auto_reconnect = true,
        .default_ap = {
            .ssid = "LabSensor-{id}",
            .password = "",
            .channel = 0,
            .max_connections = 4,
            .ip = "192.168.4.1",
            .netmask = "255.255.255.0",
            .gateway = "192.168.4.1",
            .dhcp_start = "192.168.4.2",
            .dhcp_end = "192.168.4.20",
        },
        .enable_captive_portal = true,
        .stop_ap_on_connect = true,
        .http = {
            .enable = true,
            .api_base_path = "/api/wifi",
            .enable_auth = false,
        },
        .mdns = {
            .enable = true,
            .hostname = "labsensor-{id}",
        },
    };

    ret = wifi_manager_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi manager init failed: %s", esp_err_to_name(ret));
        return;
    }

    s_wifi_manager_started = true;
    s_wifi_connected = wifi_manager_is_connected() ? 1 : 0;

    if (s_wifi_reconnect_task_handle == NULL) {
        BaseType_t task_ret = xTaskCreatePinnedToCore(wifi_reconnect_task,
                                                      "wifi_reconnect",
                                                      3072,
                                                      NULL,
                                                      3,
                                                      &s_wifi_reconnect_task_handle,
                                                      0);
        if (task_ret != pdPASS) {
            ESP_LOGW(TAG, "Failed to create WiFi reconnect task");
            s_wifi_reconnect_task_handle = NULL;
        }
    }

    ESP_LOGI(TAG, "WiFi AP config portal ready");
    ESP_LOGI(TAG, "If not connected, join AP 'LabSensor-XXXXXX' and open http://192.168.4.1/");
}

uint8_t Wifi_IS_Connected(void)
{
    if (s_wifi_manager_started) {
        s_wifi_connected = wifi_manager_is_connected() ? 1 : 0;
    }
    return s_wifi_connected;
}

void Wait_Wifi_Connected(void)
{
    if (!s_wifi_manager_started) {
        ESP_LOGW(TAG, "WiFi manager is not started");
        return;
    }

    if (!Wifi_IS_Connected()) {
        esp_err_t connect_ret = wifi_manager_connect(NULL);
        if (connect_ret != ESP_OK) {
            ESP_LOGW(TAG, "WiFi connect request failed: %s", esp_err_to_name(connect_ret));
        }
    }

    esp_err_t ret = wifi_manager_wait_connected(30000);
    if (ret == ESP_OK) {
        s_wifi_connected = 1;
        ESP_LOGI(TAG, "WiFi connected");
    } else {
        s_wifi_connected = 0;
        ESP_LOGW(TAG, "WiFi not connected yet; AP portal remains available at http://192.168.4.1/");
    }
}

void ConnectToWifi(const char *WIFI_SSID, const char *WIFI_PASW)
{
    (void)WIFI_PASW;

    if (!s_wifi_manager_started) {
        Wifi_Init();
    }

    if (WIFI_SSID != NULL && WIFI_SSID[0] != '\0') {
        ESP_LOGW(TAG, "Direct SSID connection is deprecated; configure WiFi through AP WebUI");
    }

    Wait_Wifi_Connected();
}
