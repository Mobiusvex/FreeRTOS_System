#ifndef __DRIVER_ESP8266_H__
#define __DRIVER_ESP8266_H__

#include "stdint.h"
#include "sys_defs.h"

SYS_StatusTypeDef ESP8266_Init(void);
SYS_StatusTypeDef ESP8266_Reset(void);
#endif
