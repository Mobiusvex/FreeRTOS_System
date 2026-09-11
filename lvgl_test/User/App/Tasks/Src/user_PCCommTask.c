#include "user_PCCommTask.h"
#include "FreeRTOS.h"
#include "stream_buffer.h"
#include "cmsis_os2.h"
#include "driver_w25q64.h"
#include "string.h"

#define PCCOM_TASK_PERIOD 20
#define PC_TASK_BUFFER_SIZE 512

extern StreamBufferHandle_t xPCStreamBuffer;
void user_PCCommTask(void *pvParameters) {
    static uint8_t buffer[PC_TASK_BUFFER_SIZE];
    memset(buffer, 0x34, PC_TASK_BUFFER_SIZE); // Initialize buffer to 0
    uint32_t buf_len = 0;
    BSP_W25Qxx_SectorErase(W25Q64_START_ADDR);
    BSP_W25Qxx_BufferWrite(buffer, W25Q64_START_ADDR, 512);
    memset(buffer, 0x00, PC_TASK_BUFFER_SIZE);             // Initialize buffer to 0
    BSP_W25Qxx_BufferRead(buffer, W25Q64_START_ADDR, 512); // Read back the data
    while (1) {
        uint32_t tick = osKernelGetTickCount();
        buf_len = xStreamBufferReceive(xPCStreamBuffer, buffer, 512, 0);
        tick += PCCOM_TASK_PERIOD;
        osDelayUntil(tick);
    }
}