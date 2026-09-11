#include "user_TaskInit.h"
#include "user_HardwareInitTask.h"
#include "user_LvglTask.h"
#include "user_sensorDataUpdateTask.h"
#include "user_uart3ReceiveTask.h"
#include "user_uart1ReceiveTask.h"
#include "user_ESP8266CommTask.h"
#include "debug_MonitorTask.h"
#include "user_sysDataStorageTask.h"
#include "user_PCCommTask.h"
#include "debug_func.h"

StreamBufferHandle_t xESP8266StreamBuffer = NULL;
StreamBufferHandle_t xPCStreamBuffer = NULL;
osMessageQueueId_t xESP8266CmdQueue = NULL;
osMessageQueueId_t xCmdDisplayQueue = NULL;
osMessageQueueId_t xWeatherCityQueue = NULL;

osThreadId_t user_HardwareInitTaskHandle;
const osThreadAttr_t user_HardwareInitTaskAttr = {
    .name = "HardwareInitTask",
    .stack_size = 1024 * 4,
    .priority = (osPriority_t)osPriorityHigh,
};

osThreadId_t user_LvHandlerTaskHandle;
const osThreadAttr_t user_LvHandlerTaskAttr = {
    .name = "LvHandlerTask",
    .stack_size = 1024 * 3 + 512,
    .priority = (osPriority_t)osPriorityLow2,
};

osThreadId_t user_sensorDataUpdateHandle;
const osThreadAttr_t user_sensorDataUpdateTaskAttr = {
    .name = "sensorDataUpdateTask",
    .stack_size = 1024,
    .priority = (osPriority_t)osPriorityLow3,
};

osThreadId_t user_uart3ReceiveTaskHandle;
const osThreadAttr_t user_uart3ReceiveTaskAttr = {
    .name = "uart3ReceiveTask",
    .stack_size = 512,
    .priority = (osPriority_t)osPriorityAboveNormal,
};
osThreadId_t user_uart1ReceiveTaskHandle;
const osThreadAttr_t user_uart1ReceiveTaskAttr = {
    .name = "uart1ReceiveTask",
    .stack_size = 512,
    .priority = (osPriority_t)osPriorityAboveNormal,
};
osThreadId_t user_ESP8266CommTaskHandle;
const osThreadAttr_t user_ESP8266CommTaskAttr = {
    .name = "ESP8266CommTask",
    .stack_size = 1324,
    .priority = (osPriority_t)osPriorityLow4,
};
osThreadId_t user_sysDataStorageTaskHandle;
const osThreadAttr_t user_sysDataStorageTaskAttr = {
    .name = "sysDataStorageTask",
    .stack_size = 1024,
    .priority = (osPriority_t)osPriorityLow1,
};
osThreadId_t debug_MonitorTaskHandle;
const osThreadAttr_t debug_MonitorTaskAttr = {
    .name = "monitorTask",
    .stack_size = 512,
    .priority = (osPriority_t)osPriorityLow,
};

osThreadId_t user_PCCommTaskHandle;
const osThreadAttr_t user_PCCommTaskAttr = {
    .name = "PCCommTask",
    .stack_size = 1024 + 512,
    .priority = (osPriority_t)osPriorityLow5,
};

/**
 * @brief Initialize all tasks
 * @param None
 * @retval None
 */
void userTasksInit(void) {
    xESP8266StreamBuffer = xStreamBufferCreate(512, 1);
    if (xESP8266StreamBuffer == NULL) {
        RTT_PRINTF("UART3 Failed to create xESP8266StreamBuffer\n");
    }
    xPCStreamBuffer = xStreamBufferCreate(512, 1);
    if (xPCStreamBuffer == NULL) {
        RTT_PRINTF("UART1 Failed to create xPCStreamBuffer\n");
    }

    xESP8266CmdQueue = osMessageQueueNew(10, 1, NULL);
    if (xESP8266CmdQueue == NULL) {
        RTT_PRINTF("Failed to create xESP8266CmdQueue\n");
    }
    xCmdDisplayQueue = osMessageQueueNew(20, 1, NULL);
    if (xCmdDisplayQueue == NULL) {
        RTT_PRINTF("Failed to create xCmdDisplayQueue\n");
    }
    xWeatherCityQueue = osMessageQueueNew(1, 12, NULL);
    if (xWeatherCityQueue == NULL) {
        RTT_PRINTF("Failed to create xWeatherCityQueue\n");
    }
    user_HardwareInitTaskHandle = osThreadNew(hardwareInitTask, NULL, &user_HardwareInitTaskAttr);
    if (user_HardwareInitTaskHandle == NULL) {
        RTT_PRINTF("Failed to create HardwareInitTask\n");
    }
    user_LvHandlerTaskHandle = osThreadNew(lvHandlerTask, NULL, &user_LvHandlerTaskAttr);
    if (user_LvHandlerTaskHandle == NULL) {
        RTT_PRINTF("Failed to create LvHandlerTask\n");
    }
    user_sensorDataUpdateHandle = osThreadNew(sensorDataUpdateTask, NULL, &user_sensorDataUpdateTaskAttr);
    if (user_sensorDataUpdateHandle == NULL) {
        RTT_PRINTF("Failed to create sensorDataUpdateTask\n");
    }
    user_uart3ReceiveTaskHandle = osThreadNew(uart3ReceiveTask, NULL, &user_uart3ReceiveTaskAttr);
    if (user_uart3ReceiveTaskHandle == NULL) {
        RTT_PRINTF("Failed to create uart3ReceiveTask\n");
    }
    user_uart1ReceiveTaskHandle = osThreadNew(uart1ReceiveTask, NULL, &user_uart1ReceiveTaskAttr);
    if (user_uart1ReceiveTaskHandle == NULL) {
        RTT_PRINTF("Failed to create uart1ReceiveTask\n");
    }
    user_ESP8266CommTaskHandle = osThreadNew(user_ESP8266CommTask, NULL, &user_ESP8266CommTaskAttr);
    if (user_ESP8266CommTaskHandle == NULL) {
        RTT_PRINTF("Failed to create ESP8266CommTask\n");
    }
    user_sysDataStorageTaskHandle = osThreadNew(user_sysDataStorageTask, NULL, &user_sysDataStorageTaskAttr);
    if (user_sysDataStorageTaskHandle == NULL) {
        RTT_PRINTF("Failed to create sysDataStorageTask\n");
    }
    user_PCCommTaskHandle = osThreadNew(user_PCCommTask, NULL, &user_PCCommTaskAttr);
    if (user_PCCommTaskHandle == NULL) {
        RTT_PRINTF("Failed to create PCCommTask\n");
    }
#if (DEBUG_FUNC_ENABLE == 1)
    debug_MonitorTaskHandle = osThreadNew(debug_MonitorTask, NULL, &debug_MonitorTaskAttr);
    if (debug_MonitorTaskHandle == NULL) {
        RTT_PRINTF("Failed to create debug_MonitorTask\n");
    }
#endif
}
