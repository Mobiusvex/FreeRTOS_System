#include "user_AudioTask.h"
#include "cmsis_os2.h"
#include "HWDataAccess.h"
#include "debug_func.h"
#include "cmd_audio.h"

/* ---------- 播放一个文件，返回打断它的新 slot（正常播完返回 NONE） ---------- */

extern osMessageQueueId_t xAudioCmdQueue;

static uint8_t play_one(uint8_t slot);

void user_AudioTask(void *pvParameters) {
    enum_slot_t slot = AUDIO_SLOT_NONE;
    while (1) {
        if (osMessageQueueGet(xAudioCmdQueue, &slot, NULL, osWaitForever) == osOK) {
            enum_slot_t next;
            while ((next = HW_Interface.AUDIO.PlayVoice(slot)) != AUDIO_SLOT_NONE) {
                HW_Interface.AUDIO.StopVoice(); /* 被打断：停旧的 */
                slot = next;                    /* 接着播新的 */
            }
            HW_Interface.AUDIO.StopVoice();
        }
    }
}
