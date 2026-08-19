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
            SEGGER_RTT_printf(0, "Received PC: %d bytes\n", len); // 输出接收到的字节数
            BSP_UART_Transmit_Block(BSP_UART_ESP8266, buffer, len, 100);
        }
    }
}

// NOTE: ESP8266接收
// 1.云平台通过ESP8266主动下发数据给STM32
// （1）接收任务解析数据，并进行相应的处理
// 2.STM32发送指令给ESP8266,并等待ESP8266回复数据
// （1）STM32指令标志置1，等待回复对应事件组标志位
// （2）接收任务判断如果指令标志置1，则通过列队发送给指令任务