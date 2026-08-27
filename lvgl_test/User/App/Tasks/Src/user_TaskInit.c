#include "user_TaskInit.h"
#include "user_HardwareInitTask.h"
#include "user_LvglTask.h"
#include "user_sensorDataUpdateTask.h"
#include "user_uart3ReceiveTask.h"
#include "user_uart1ReceiveTask.h"
#include "user_ESP8266CommTask.h"

#include "SEGGER_RTT.h"

StreamBufferHandle_t xESP8266StreamBuffer = NULL;
osMessageQueueId_t xESP8266CmdQueue = NULL;

osThreadId_t user_HardwareInitTaskHandle;
const osThreadAttr_t user_HardwareInitTaskAttr = {
    .name = "HardwareInitTask",
    .stack_size = 1024,
    .priority = (osPriority_t)osPriorityHigh,
};

osThreadId_t user_LvHandlerTaskHandle;
const osThreadAttr_t user_LvHandlerTaskAttr = {
    .name = "LvHandlerTask",
    .stack_size = 2048,
    .priority = (osPriority_t)osPriorityLow,
};

osThreadId_t user_sensorDataUpdateHandle;
const osThreadAttr_t user_sensorDataUpdateTaskAttr = {
    .name = "sensorDataUpdateTask",
    .stack_size = 1024,
    .priority = (osPriority_t)osPriorityLow1,
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
    .name = "uartESP8266CommTask",
    .stack_size = 650,
    .priority = (osPriority_t)osPriorityLow2,
};

/**
 * @brief Initialize all tasks
 * @param None
 * @retval None
 */
void userTasksInit(void) {
    xESP8266StreamBuffer = xStreamBufferCreate(1024, 1);
    if (xESP8266StreamBuffer == NULL) {
        SEGGER_RTT_printf(0, "UART3 Failed to create xESP8266StreamBuffer\n");
    }

    xESP8266CmdQueue = osMessageQueueNew(5, 1, NULL);
    if (xESP8266CmdQueue == NULL) {
        SEGGER_RTT_printf(0, "Failed to create xESP8266CmdQueue\n");
    }

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
    user_uart3ReceiveTaskHandle = osThreadNew(uart3ReceiveTask, NULL, &user_uart3ReceiveTaskAttr);
    if (user_uart3ReceiveTaskHandle == NULL) {
        SEGGER_RTT_printf(0, "Failed to create uart3ReceiveTask\n");
    }
    user_uart1ReceiveTaskHandle = osThreadNew(uart1ReceiveTask, NULL, &user_uart1ReceiveTaskAttr);
    if (user_uart1ReceiveTaskHandle == NULL) {
        SEGGER_RTT_printf(0, "Failed to create uart1ReceiveTask\n");
    }
    user_ESP8266CommTaskHandle = osThreadNew(user_ESP8266CommTask, NULL, &user_ESP8266CommTaskAttr);
    if (user_ESP8266CommTaskHandle == NULL) {
        SEGGER_RTT_printf(0, "Failed to create ESP8266CommTask\n");
    }
}
