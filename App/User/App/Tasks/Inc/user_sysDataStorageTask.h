#ifndef USER_SYS_DATA_STORAGE_TASK_H
#define USER_SYS_DATA_STORAGE_TASK_H
#include "stdint.h"

#define FLAG_MSG_DATA_STORAGE (0X0001)
void user_sysDataStorageTask(void *pvParameters);
#endif // USER_SYS_DATA_STORAGE_TASK_H