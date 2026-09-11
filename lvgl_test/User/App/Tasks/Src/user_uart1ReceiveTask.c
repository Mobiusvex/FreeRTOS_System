#include "user_uart1ReceiveTask.h"
#include "bsp_uart.h"
#include "debug_func.h"
#include "FreeRTOS.h"
#include "task.h"
#include "debug_func.h"
#include "stream_buffer.h"

extern StreamBufferHandle_t xPCStreamBuffer;
void uart1ReceiveTask(void *pvParameters) {
    BSP_UART_Init(BSP_UART_PC, NULL);
    BSP_UART_RegisterTask(BSP_UART_PC, osThreadGetId());
    uint32_t ulFlags;
    static uint8_t buffer[512];
    uint32_t buf_len = 0, stream_len = 0;
    while (1) {
        ulFlags = osThreadFlagsWait(0x01,
                                    osFlagsWaitAny,
                                    osWaitForever);

        if (ulFlags & 0x01) {
            buf_len = BSP_UART_ReadFromBuffer(BSP_UART_PC, buffer, sizeof(buffer));
            // BSP_UART_Transmit_Block(BSP_UART_ESP8266, buffer, len, 100);
        }
        if (buf_len > 0) {
            stream_len = xStreamBufferSend(xPCStreamBuffer, buffer, buf_len, 10);
            if (stream_len < buf_len) {
                // 极端情况：流缓冲区满了，丢包
                RTT_PRINTF("Warning: PC Stream Buffer Full! Lost %d bytes\n", buf_len - stream_len);
            }
        }
    }
}
