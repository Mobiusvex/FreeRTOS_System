#ifndef __USER_UART_RECEIVE_TASK_H__
#define __USER_UART_RECEIVE_TASK_H__
#include "stdint.h"
#include "cmsis_os2.h"

void uartReceiveTask(void *pvParameters);
#endif // __USER_UART_RECEIVE_TASK_H__