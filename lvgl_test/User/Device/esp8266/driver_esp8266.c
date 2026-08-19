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
