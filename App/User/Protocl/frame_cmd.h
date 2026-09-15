#ifndef FRAME_CMD_H
#define FRAME_CMD_H

/* ============ OTA 相关 ============ */
#define CMD_OTA_START 0x11 /* 上位机→STM32：起始包 */
#define CMD_OTA_DATA 0x12  /* 上位机→STM32：数据包 */
#define CMD_OTA_ACK 0x13   /* STM32→上位机：应答(0x66成功/0x55失败) */
#define CMD_OTA_END 0x14   /* 上位机→STM32：结束包 */

/* ============ 通用命令 ============ */
// #define CMD_QUERY_STATUS     0x20    /* 查询设备状态 */
// #define CMD_SET_PARAM        0x21    /* 设置参数 */
// #define CMD_GET_PARAM        0x22    /* 读取参数 */
// #define CMD_REBOOT           0x23    /* 重启设备 */

/* ============ ACK 数据字段 ============ */
#define ACK_OK 0x66
#define ACK_FAIL 0x50
#define ACK_PARAM_ERROR 0x51
#define ACK_OTA_SIZE_ERROR 0x52
#define ACK_CMD_ERROR 0x53
#define ACK_FLASH_ERROR 0x54
#define ACK_PAGE_CRC_ERROR 0x55
#define ACK_PAKET_CRC_ERROR 0x56
#define ACK_FRAME_LACK 0x57

#endif /* FRAME_CMD_H */