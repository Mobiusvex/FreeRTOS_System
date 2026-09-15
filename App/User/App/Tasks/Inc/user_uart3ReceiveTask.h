#ifndef __USER_UART3_RECEIVE_TASK_H__
#define __USER_UART3_RECEIVE_TASK_H__
#include "stdint.h"
#include "cmsis_os2.h"

void uart3ReceiveTask(void *pvParameters);
#endif // __USER_UART3_RECEIVE_TASK_H__