#ifndef DEBUG_FUNC_H
#define DEBUG_FUNC_H
#include "FreeRTOS.h"
#include "task.h"

#define DEBUG_FUNC_ENABLE 0

void printTaskStackRemainingCapacity(TaskHandle_t xTaskHandle, uint32_t period_ms, uint32_t *p_last_time);

#endif // DEBUG_FUNC_H