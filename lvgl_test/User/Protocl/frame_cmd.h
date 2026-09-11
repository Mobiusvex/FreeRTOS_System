#ifndef FRAME_CMD_H
#define FRAME_CMD_H

/* 所有指令集中定义，业务层和协议层共用 */
#define CMD_OTA_START 0x11
#define CMD_OTA_DATA 0x12
#define CMD_OTA_ACK 0x13
#define CMD_OTA_END 0x14
#define CMD_QUERY_STATUS 0x20
#define CMD_SET_PARAM 0x21

#endif // FRAME_CMD_H