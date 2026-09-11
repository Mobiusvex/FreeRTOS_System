#ifndef FRAME_H
#define FRAME_H

/* 帧的最大长度（含头尾） */
#define FRAME_MAX_SIZE (1 + 2 + 1 + 1 + 256 + 2 + 1) /* = 264 */

/* 解析结果 */
typedef struct {
    uint16_t seq;      /* 序号 */
    uint8_t cmd;       /* 指令 */
    uint8_t data_len;  /* 数据长度 */
    uint8_t data[256]; /* 数据体 */
} Frame_t;

/* 组帧结果状态 */
typedef enum {
    FRAME_OK = 0,
    FRAME_INCOMPLETE, /* 数据不够，继续等 */
    FRAME_CRC_ERROR,  /* CRC校验失败 */
    FRAME_INVALID,    /* 帧头/帧尾/长度非法 */
} FrameStatus_t;

/* 从字节流中尝试解析一帧；成功则填充out，返回FRAME_OK；同时更新consumed */
FrameStatus_t Frame_Parse(const uint8_t *buf, uint16_t len,
                          Frame_t *out, uint16_t *consumed);

/* 组装一帧（用于发送），返回总字节数 */
uint16_t Frame_Build(uint8_t *out, uint16_t seq, uint8_t cmd,
                     const uint8_t *data, uint8_t data_len);

#endif