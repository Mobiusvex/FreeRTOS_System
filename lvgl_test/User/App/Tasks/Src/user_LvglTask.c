#include "user_LVGLTask.h"
#include "lvgl.h"
#include "FreeRTOS.h"
#include "task.h"
#include "SEGGER_RTT.h"
#include "debug_func.h"

/**
 * @brief LVGL task handler
 * @param pvParameters Task parameter (not used)
 * @retval None
 */
void lvHandlerTask(void *pvParameters) {
    while (1) {
        osDelay(5);
        lv_timer_handler();
        // 调试：任务剩余栈空间打印
        static uint32_t last_print_time = 0;
        printTaskStackRemainingCapacity((TaskHandle_t)osThreadGetId(), 10000, &last_print_time);
    }
}