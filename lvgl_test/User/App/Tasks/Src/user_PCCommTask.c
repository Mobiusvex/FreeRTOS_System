#include "user_PCCommTask.h"
#include "FreeRTOS.h"
#include "stream_buffer.h"
#include "cmsis_os2.h"
#include "driver_w25q64.h"
#include "string.h"
#include "bsp_uart.h"
#include "frame.h"
#include "cmd_manager.h"
#include "debug_func.h"
#include "cmd_ota.h"
#include "sys_data.h"

#define PCCOM_TASK_PERIOD 5
#define PC_TASK_BUFFER_SIZE 300
#define PAKET_UPDATE_TIME 3000
static FrameParser_t s_parser;
extern StreamBufferHandle_t xPCStreamBuffer;
extern osMessageQueueId_t xCmdDisplayQueue;

void ota_display_clean();

void user_PCCommTask(void *pvParameters) {
    FrameParser_Init(&s_parser);
    static uint8_t buffer[PC_TASK_BUFFER_SIZE];
    Frame_t frame;
    uint32_t buf_len = 0;
    uint32_t update_time = 0;
    bool ota_clean = false;
    while (1) {
        uint32_t tick = osKernelGetTickCount();
        buf_len = xStreamBufferReceive(xPCStreamBuffer, buffer, PC_TASK_BUFFER_SIZE, 0);
        if (buf_len > 0) {
            // 处理
            FrameStatus_t st = FrameParser_Feed(&s_parser, buffer, buf_len, &frame);
            while (st == FRAME_OK) {
                ota_clean = true;
                Cmd_Dispatch(&frame);
                st = FrameParser_Feed(&s_parser, NULL, 0, &frame);
                ota_sys_data_update();
                update_time = osKernelGetTickCount();
            }
            if (st == FRAME_CRC_ERROR) {
                RTT_PRINTF("Frame CRC error, resyncing...\n");
            }
        }

        if (ota_clean && (osKernelGetTickCount() - update_time) > PAKET_UPDATE_TIME) {
            ota_clean = false;
            ota_display_clean();
        }
        tick += PCCOM_TASK_PERIOD;
        osDelayUntil(tick);
    }
}
