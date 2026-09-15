#include "user_LVGLTask.h"
#include "lvgl.h"
#include "FreeRTOS.h"
#include "task.h"
#include "debug_func.h"
#include "debug_func.h"
#include "sys_data.h"
#include "user_ui_updata.h"
#include "user_ui_popup.h"

extern osMessageQueueId_t xCmdDisplayQueue;
/**
 * @brief LVGL task handler
 * @param pvParameters Task parameter (not used)
 * @retval None
 */
void lvHandlerTask(void *pvParameters) {
    SystemGlobalData_t sys_data;
    SYS_DataEventType_t next_refresh_cmd;

    SYS_DATA_GetSnapshot(&sys_data); // 获取系统数据
    user_ui_refresh_all(&sys_data);  // 刷新UI
    while (1) {
        osDelay(5);
        lv_timer_handler();

        if (osMessageQueueGet(xCmdDisplayQueue, &next_refresh_cmd, 0, 0) == osOK) {
            SYS_DATA_GetSnapshot(&sys_data); // 获取系统数据
            user_ui_updata(next_refresh_cmd, &sys_data);
        }
    }
}