#ifndef MQTT_CONNECT_H
#define MQTT_CONNECT_H


void ConnectToMqtt(const char * BROKER_URL ,const char* UserName,const char* Password);                        
void Mqtt_Subscribe_Topic(const char *topic,int qos);
void Mqtt_Dissubscribe_Topic(const char *topic);
uint8_t Mqtt_Is_Connectecd(void);
void Mqtt_Publish_Message(const char *topic, const char *data, int len, int qos, int retain);
#endif
