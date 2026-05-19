#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H


void Wifi_Init(void);

void ConnectToWifi(const char* WIFI_SSID, const char* WIFI_PASW);

uint8_t Wifi_IS_Connected(void);

#endif