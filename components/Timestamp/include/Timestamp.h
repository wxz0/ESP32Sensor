#ifndef TIMESTAMP_H
#define TIMESTAMP_H

#include "stdint.h"
#include "esp_err.h"
#include "time.h"

esp_err_t SNTP_Sync_Time(void);
time_t SNTP_Get_Timestamp(void);

#endif 
