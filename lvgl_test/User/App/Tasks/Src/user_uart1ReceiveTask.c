#include "user_uart1ReceiveTask.h"
#include "bsp_uart.h"
#include "SEGGER_RTT.h"

void uart1ReceiveTask(void *pvParameters) {
    BSP_UART_Init(BSP_UART_PC, NULL);
    BSP_UART_RegisterTask(BSP_UART_PC, osThreadGetId());
    uint32_t ulFlags;
    uint8_t buffer[512];
    uint16_t index = 0;
    while (1) {
        ulFlags = osThreadFlagsWait(0x01,
                                    osFlagsWaitAny,
                                    osWaitForever);

        if (ulFlags & 0x01) {
            uint16_t len = BSP_UART_ReadFromBuffer(BSP_UART_PC, buffer, sizeof(buffer));
            BSP_UART_Transmit_Block(BSP_UART_ESP8266, buffer, len, 100);
        }
    }
}
