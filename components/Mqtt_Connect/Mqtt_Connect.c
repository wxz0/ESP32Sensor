#include <stdio.h>
#include "Mqtt_Connect.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "Mqtt_Connect";
esp_mqtt_client_handle_t client = NULL;
uint8_t Mqtt_Connect_State = 0; // 0: 未连接，1：已连接

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        Mqtt_Connect_State = 1;
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        break;
    case MQTT_EVENT_DISCONNECTED:
        Mqtt_Connect_State = 0;
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        /*处理接收到的数据*/
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) 
        {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void ConnectToMqtt(const char * BROKER_URL ,const char* UserName,const char* Password)
{
    bool broker_is_uri = (BROKER_URL != NULL) && (strstr(BROKER_URL, "://") != NULL);

    esp_mqtt_client_config_t mqtt_cfg = 
    {
        .network.disable_auto_reconnect = false,
        .network.reconnect_timeout_ms = 10000,
        .broker.address.uri = broker_is_uri ? (const char *)BROKER_URL : NULL,
        .broker.address.hostname = broker_is_uri ? NULL : (const char *)BROKER_URL,
        .broker.address.port = broker_is_uri ? 0U : (uint32_t)1883,
        .broker.address.transport = broker_is_uri ? MQTT_TRANSPORT_UNKNOWN : MQTT_TRANSPORT_OVER_TCP,
        .credentials.client_id = "Lab_Sensor_Client",
        .credentials.username = UserName,
        .credentials.authentication.password = Password,
    };

    ESP_LOGI(TAG,
             "MQTT broker mode=%s target=%s",
             broker_is_uri ? "uri" : "hostname",
             (BROKER_URL != NULL) ? BROKER_URL : "<null>");

    client = esp_mqtt_client_init(&mqtt_cfg);
    
    if(client == NULL)
    {
        ESP_LOGE(TAG, "Failed to create MQTT client");
        return;
    }

    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    
    esp_mqtt_client_start(client);
}

uint8_t Mqtt_Is_Connectecd(void)
{
    return Mqtt_Connect_State;
}

void Mqtt_Subscribe_Topic(const char *topic,int qos)
{
  esp_mqtt_client_subscribe_single(client,topic,qos);
}

void Mqtt_Dissubscribe_Topic(const char *topic)
{
  esp_mqtt_client_unsubscribe(client,topic);
}

void Mqtt_Publish_Message(const char *topic, const char *data, int len, int qos, int retain)
{
  esp_mqtt_client_publish(client,topic,data,len,qos,retain);
}

