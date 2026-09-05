#include "user_sysDataStorageTask.h"
#include "sys_data.h"
#include "cmsis_os2.h"
void user_sysDataStorageTask(void *pvParameters) {
    uint32_t flags;
    while (1) {
        flags = osThreadFlagsWait(FLAG_MSG_DATA_STORAGE, osFlagsWaitAny, osWaitForever);
        if (flags & FLAG_MSG_DATA_STORAGE) {
            SYS_DATA_Save();
        }
    }
}