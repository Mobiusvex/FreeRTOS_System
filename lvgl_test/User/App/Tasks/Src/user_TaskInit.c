#include "user_TaskInit.h"
#include "user_HardwareInitTask.h"
#include "user_LvglTask.h"
#include "user_sensorDataUpdateTask.h"
#include "user_uartReceiveTask.h"

#include "SEGGER_RTT.h"

osThreadId_t user_HardwareInitTaskHandle;
const osThreadAttr_t user_HardwareInitTaskAttr = {
    .name = "HardwareInitTask",
    .stack_size = 1024,
    .priority = (osPriority_t)osPriorityHigh,
};

osThreadId_t user_LvHandlerTaskHandle;
const osThreadAttr_t user_LvHandlerTaskAttr = {
    .name = "LvHandlerTask",
    .stack_size = 1024,
    .priority = (osPriority_t)osPriorityLow,
};

osThreadId_t user_sensorDataUpdateHandle;
const osThreadAttr_t user_sensorDataUpdateTaskAttr = {
    .name = "sensorDataUpdateTask",
    .stack_size = 1024,
    .priority = (osPriority_t)osPriorityLow1,
};

osThreadId_t user_uartReceiveTaskHandle;
const osThreadAttr_t user_uartReceiveTaskAttr = {
    .name = "uartReceiveTask",
    .stack_size = 1024,
    .priority = (osPriority_t)osPriorityAboveNormal,
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
    user_sensorDataUpdateHandle = osThreadNew(sensorDataUpdateTask, NULL, &user_sensorDataUpdateTaskAttr);
    if (user_sensorDataUpdateHandle == NULL) {
        SEGGER_RTT_printf(0, "Failed to create sensorDataUpdateTask\n");
    }
    user_uartReceiveTaskHandle = osThreadNew(uartReceiveTask, NULL, &user_uartReceiveTaskAttr);
    if (user_uartReceiveTaskHandle == NULL) {
        SEGGER_RTT_printf(0, "Failed to create uartReceiveTask\n");
    }
}
