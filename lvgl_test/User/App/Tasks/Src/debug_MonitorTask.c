#include "debug_MonitorTask.h"
#include "user_ESP8266CommTask.h"
#include "user_LvglTask.h" // 注意：这里需要包含 LVGL 任务的头文件
#include "user_sensorDataUpdateTask.h"
#include "user_uart1ReceiveTask.h"
#include "user_uart3ReceiveTask.h"
#include "debug_func.h"

extern osThreadId_t user_LvHandlerTaskHandle;
extern osThreadId_t user_sensorDataUpdateHandle;
extern osThreadId_t user_uart1ReceiveTaskHandle;
extern osThreadId_t user_uart3ReceiveTaskHandle;
extern osThreadId_t user_ESP8266CommTaskHandle;
extern osThreadId_t user_PCCommTaskHandle;

void debug_MonitorTask(void *pvParameters) {
    const TickType_t xDelay = pdMS_TO_TICKS(5000); // 每 5 秒更新一次
    TickType_t xLastWakeTime = xTaskGetTickCount();
    static UBaseType_t uxWaterMark[7];
    // 注意：监控任务自身栈极小，千万不要在这里定义大数组（如 char buf[256]）
    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xDelay);

        if (user_LvHandlerTaskHandle != NULL) {
            uxWaterMark[0] = uxTaskGetStackHighWaterMark(user_LvHandlerTaskHandle);
        }
        if (user_sensorDataUpdateHandle != NULL) {
            uxWaterMark[1] = uxTaskGetStackHighWaterMark(user_sensorDataUpdateHandle);
        }
        if (user_uart1ReceiveTaskHandle != NULL) {
            uxWaterMark[2] = uxTaskGetStackHighWaterMark(user_uart1ReceiveTaskHandle);
        }
        if (user_uart3ReceiveTaskHandle != NULL) {
            uxWaterMark[3] = uxTaskGetStackHighWaterMark(user_uart3ReceiveTaskHandle);
        }
        if (user_ESP8266CommTaskHandle != NULL) {
            uxWaterMark[4] = uxTaskGetStackHighWaterMark(user_ESP8266CommTaskHandle);
        }
        if (user_PCCommTaskHandle != NULL) {
            uxWaterMark[5] = uxTaskGetStackHighWaterMark(user_PCCommTaskHandle);
        }
        // 打印监控任务自身（用于调试监控任务是否爆栈）
        uxWaterMark[6] = uxTaskGetStackHighWaterMark(NULL);
        HeapStats_t stats = {0};
        vPortGetHeapStats(&stats);

        // RTT_PRINTF("Maximum contiguous free block: %d B\r\n", stats.xSizeOfLargestFreeBlockInBytes);
        // RTT_PRINTF("Free blocks: %d\r\n", stats.xNumberOfFreeBlocks);
        // RTT_PRINTF("LvHandler Task Stack Free: %u words\r\n", uxWaterMark[0]);
        // RTT_PRINTF("sensorDataUpdate Task Stack Free: %u words\r\n", uxWaterMark[1]);
        // RTT_PRINTF("uart1Receive Task Stack Free: %u words\r\n", uxWaterMark[2]);
        // RTT_PRINTF("uart3Receive Task Stack Free: %u words\r\n", uxWaterMark[3]);
        // RTT_PRINTF("ESP8266Comm Task Stack Free: %u words\r\n", uxWaterMark[4]);
        // RTT_PRINTF("PCCom Task Stack Free: %u words\r\n", uxWaterMark[5]);
        // RTT_PRINTF("Monitor Task Stack Free: %u words\r\n", uxWaterMark[6]);
    }
}

void SYS_DumpTaskStats(void) {
    static char buf[1024];
    RTT_PRINTF("\n===== Task Runtime Stats =====\n");
    vTaskGetRunTimeStats(buf);
    RTT_PRINTF(buf);
    RTT_PRINTF("===============================\n");
}
void vApplicationIdleHook(void) {
    static uint32_t last_print = 0;
    uint32_t now = xTaskGetTickCount();

    /* 每 10 秒打印一次 */
    if (now - last_print >= pdMS_TO_TICKS(10000)) {
        last_print = now;
        SYS_DumpTaskStats();
    }
}
