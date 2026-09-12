#include "user_sysDataStorageTask.h"
#include "sys_data.h"
#include "cmsis_os2.h"
#include "bsp_system.h"

#define DATA_STORAGE_RETRY_MAX 10
void user_sysDataStorageTask(void *pvParameters) {
    uint32_t flags;
    uint8_t retry = 0;
    SYS_UPGRADE_t is_upgrade;
    while (1) {
        flags = osThreadFlagsWait(FLAG_MSG_DATA_STORAGE, osFlagsWaitAny, osWaitForever);
        if (flags & FLAG_MSG_DATA_STORAGE) {
            retry = 0;
            while (retry++ < DATA_STORAGE_RETRY_MAX) {
                if (SYS_DATA_Save() == SYS_OK) {
                    break;
                }
            }
            SYS_DATA_GetNewPaketState(&is_upgrade);
            if (retry < DATA_STORAGE_RETRY_MAX && is_upgrade == SYS_NEW_PAKET_READY) {
                osDelay(3000);
                BSP_SystemReset();
            } else {
                SYS_DATA_SetNewPaketState(SYS_NEW_PAKET_NULL);
            }
        }
    }
}