#include "user_TaskInit.h"
#include "user_HardwareInitTask.h"
#include "user_LvglTask.h"

#include "SEGGER_RTT.h"

osThreadId_t user_HardwareInitTaskHandle;
osThreadAttr_t user_HardwareInitTaskAttr = {
    .name = "HardwareInitTask",
    .stack_size = 1024,
    .priority = (osPriority_t)osPriorityHigh,
};

osThreadId_t user_LvHandlerTaskHandle;
;
osThreadAttr_t user_LvHandlerTaskAttr = {
    .name = "LvHandlerTask",
    .stack_size = 1024,
    .priority = (osPriority_t)osPriorityLow,
};

/**
 * @brief Initialize all tasks
 * @param None
 * @retval None
 */
void userTasksInit(void) {
    user_HardwareInitTaskHandle = osThreadNew(hardwareInitTask, NULL, &user_HardwareInitTaskAttr);
    if (user_HardwareInitTaskHandle == NULL) {
        SEGGER_RTT_printf(0, "Failed to create HardwareInitTask\n");
    }
    user_LvHandlerTaskHandle = osThreadNew(lvHandlerTask, NULL, &user_LvHandlerTaskAttr);
    if (user_LvHandlerTaskHandle == NULL) {
        SEGGER_RTT_printf(0, "Failed to create LvHandlerTask\n");
    }
}
