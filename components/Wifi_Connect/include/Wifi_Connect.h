#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H

#include <stdint.h>

void Wifi_Init(void);

void ConnectToWifi(const char* WIFI_SSID, const char* WIFI_PASW);

void Wait_Wifi_Connected(void);

uint8_t Wifi_IS_Connected(void);

#endif
