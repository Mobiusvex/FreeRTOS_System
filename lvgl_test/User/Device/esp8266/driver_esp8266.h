#ifndef __DRIVER_ESP8266_H__
#define __DRIVER_ESP8266_H__

#include "stdint.h"
#include "sys_defs.h"

typedef enum {
    HW_UART_ESP8266_AT_REPLAY,
    HW_UART_ESP8266_CONTROL,
    HW_UART_ESP8266_WEATHER,
    HW_UART_ESP8266_TIME,
} HW_UART_RX_DATA_TypeDef;

// 成员1：字符相同 成员2：子串匹配成功，成员3：子串匹配失败
typedef enum {
    STRING_MATCH_STRING_SUCCESS = 0,
    STRING_MATCH_SUBSTRING_SUCCESS = 1,
    STRING_MATCH_FAILED = 2,
} ESP8266_StringMatchResult;

SYS_StatusTypeDef ESP8266_Init(void);
SYS_StatusTypeDef ESP8266_Reset(void);
SYS_StatusTypeDef ESP8266_SendCom(const char *command, const uint16_t timeout);
#endif
