#include "driver_esp8266.h"
#include "bsp_uart.h"
#include "cmsis_os2.h"
#include "bsp_gpio.h"
#include "bsp_delay.h"
#include "SEGGER_RTT.h"
#include "string.h"

SYS_StatusTypeDef ESP8266_Init(void) {
    SYS_StatusTypeDef status = SYS_OK;
    status = BSP_UART_Init(BSP_UART_ESP8266, NULL);
    if (status != SYS_OK) {
        return status;
    }

    BSP_GPIO_Write(BSP_GPIO_ESP8266_RST, BSP_GPIO_HIGH);
    BSP_GPIO_Write(BSP_GPIO_ESP8266_EN, BSP_GPIO_HIGH);
    return status;
}

SYS_StatusTypeDef ESP8266_Reset(void) {
    BSP_GPIO_Write(BSP_GPIO_ESP8266_RST, BSP_GPIO_LOW);
    BSP_DelayMS_Sleep(500);
    BSP_GPIO_Write(BSP_GPIO_ESP8266_RST, BSP_GPIO_HIGH);
    return SYS_OK;
}

SYS_StatusTypeDef ESP8266_SendCom(const char *command) {
    SYS_StatusTypeDef status = SYS_OK;
    if (command == NULL) {
        return SYS_INVALID_PARAM;
    }
    BSP_UART_Transmit_Block(BSP_UART_ESP8266, command, strlen(command), 10);
    return status;
}

SYS_StatusTypeDef ESP8266_SendCmdWaitResponse(const char *command, const char *response) {
    SYS_StatusTypeDef status = SYS_OK;
    if (command == NULL || response == NULL) {
        return SYS_INVALID_PARAM;
    }
    ESP8266_SendCom(command);
    char buffer[256];
    return status;
}

ESP8266_StringMatchResult ESP8266_StringMatch(const char *str, const char *pattern) {
    ESP8266_StringMatchResult result = STRING_MATCH_FAILED;
    if (str == NULL || pattern == NULL) {
        return result;
    }
    if (strcmp(str, pattern) == 0) {
        result = STRING_MATCH_STRING_SUCCESS;
    } else if (strstr(str, pattern) != NULL) {
        result = STRING_MATCH_SUBSTRING_SUCCESS;
    }
    return result;
}
