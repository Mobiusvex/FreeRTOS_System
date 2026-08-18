#include "user_uartReceiveTask.h"
#include "bsp_uart.h"
#include "SEGGER_RTT.h"

void uartReceiveTask(void *pvParameters) {
    osEventFlagsId_t evt_group = BSP_UART_GetEventGroup();
    if (evt_group == NULL) {
        return;
    }
    uint32_t wait_mask = BSP_UART_ESP8266_EVENT_MASK;
    uint8_t buffer[512];
    uint16_t index = 0;
    while (1) {
        uint32_t event_flags = osEventFlagsWait(evt_group, wait_mask, osFlagsWaitAny, osWaitForever);
        if (event_flags & BSP_UART_ESP8266_EVENT_MASK) {
            SEGGER_RTT_printf(0, "%d\n", index++);
            uint16_t len = BSP_UART_ReadFromBuffer(BSP_UART_ESP8266, buffer, sizeof(buffer));
            BSP_UART_Transmit_Block(BSP_UART_ESP8266, buffer, len, 100);
            SEGGER_RTT_printf(0, "Received %d bytes\n", len); // 输出接收到的字节数
        }
    }
}