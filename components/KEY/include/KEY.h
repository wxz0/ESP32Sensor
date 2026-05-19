#ifndef KEY_H
#define KEY_H

#include "App_Data.h"
#define Key_State_t uint8_t

void KEY_Init(void);

Key_State_t KEY_GetState(void);

KEY_x KEY_GetNum(void);


#endif
