#include "user_uart3ReceiveTask.h"
#include "bsp_uart.h"
#include "SEGGER_RTT.h"
#include "FreeRTOS.h"
#include "stream_buffer.h"

extern StreamBufferHandle_t xESP8266StreamBuffer;

void uart3ReceiveTask(void *pvParameters) {
    uint32_t ulFlags;
    uint8_t buffer[512];
    uint16_t index = 0;
    uint32_t buf_len = 0, stream_len = 0;
    BSP_UART_RegisterTask(BSP_UART_ESP8266, osThreadGetId());

    while (1) {
        ulFlags = osThreadFlagsWait(0x01, osFlagsWaitAny, osWaitForever);

        if (ulFlags & 0x01) {
            buf_len = BSP_UART_ReadFromBuffer(BSP_UART_ESP8266, buffer, sizeof(buffer));
            BSP_UART_Transmit_Block(BSP_UART_PC, buffer, buf_len, 100);

            if (buf_len > 0) {
                stream_len = xStreamBufferSend(xESP8266StreamBuffer, buffer, buf_len, 0); // 超时0，绝不阻塞
                if (stream_len < buf_len) {
                    // 极端情况：流缓冲区满了，丢包
                    SEGGER_RTT_printf(0, "Warning: ESP8266 Stream Buffer Full! Lost %d bytes\n", buf_len - stream_len);
                }
            }
        }
    }
}
