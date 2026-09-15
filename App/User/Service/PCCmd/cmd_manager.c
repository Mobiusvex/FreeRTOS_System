#include "cmd_manager.h"
#include "frame_cmd.h"
#include "cmd_ota.h"
#include "sys_data.h"

/* 命令处理函数原型 */
typedef void (*CmdHandler_t)(const Frame_t *frame);

typedef struct {
    uint8_t cmd;
    CmdHandler_t handler;
} CmdEntry_t;

/* 注册表 */
static const CmdEntry_t cmd_table[] = {
    {CMD_OTA_START, OTA_HandleStart},
    {CMD_OTA_DATA, OTA_HandleData},
    {CMD_OTA_END, OTA_HandleEnd},
};

/**
 * @brief 处理任务收到一帧后调用
 * @param frame 任务收到的一帧数据
 * @retval 无
 */
void Cmd_Dispatch(const Frame_t *frame) {
    for (uint8_t i = 0; i < sizeof(cmd_table) / sizeof(cmd_table[0]); i++) {
        if (cmd_table[i].cmd == frame->cmd) {
            cmd_table[i].handler(frame);
            return;
        }
    }
}