#include "user_sysDataStorageTask.h"
#include "sys_data.h"
#include "cmsis_os2.h"
void user_sysDataStorageTask(void *pvParameters) {
    while (1) {
        osDelay(1000); // Delay for 1 second
    }
}