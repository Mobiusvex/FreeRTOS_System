#include "debug_func.h"

#include "SEGGER_RTT.h"

void printTaskStackRemainingCapacity(TaskHandle_t xTaskHandle, uint32_t period_ms, uint32_t *p_last_time) {
#if DEBUG_FUNC_ENABLE
    if (xTaskHandle == NULL || p_last_time == NULL) return;

    uint32_t now = xTaskGetTickCount();

    // period_ms == 0 表示每次都打印
    if (period_ms == 0 || (now - *p_last_time) >= pdMS_TO_TICKS(period_ms)) {
        UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(xTaskHandle);
        SEGGER_RTT_printf(0, "Task %s remaining stack: %u words\r\n",
                          pcTaskGetName(xTaskHandle), uxHighWaterMark);
        *p_last_time = now;
    }
#endif
}