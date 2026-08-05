#include "user_LVGLTask.h"
#include "lvgl.h"

/**
 * @brief LVGL task handler
 * @param pvParameters Task parameter (not used)
 * @retval None
 */
void lvHandlerTask(void *pvParameters) {
    while (1) {
        osDelay(5);
        lv_timer_handler();
    }
}