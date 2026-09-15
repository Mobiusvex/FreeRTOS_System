#ifndef FRAME_H
#define FRAME_H

#include <stdint.h>
#include <stdbool.h>
#include "bsp_uart.h"
/* ============ 协议常量 ============ */
#define FRAME_HEAD 0x68
#define FRAME_TAIL 0x86
#define FRAME_DATA_MAX 255
#define FRAME_HEAD_LEN 5 /* 起始 + 序号2 + 指令1 + 长度1 */
#define FRAME_CRC_LEN 2
#define FRAME_TAIL_LEN 1
#define FRAME_MAX_SIZE (FRAME_HEAD_LEN + FRAME_DATA_MAX + FRAME_CRC_LEN + FRAME_TAIL_LEN)
/* = 263 */

/* ============ 解析出的帧 ============ */
typedef struct {
    uint16_t seq;                 /* 序号（小端读出） */
    uint8_t cmd;                  /* 指令 */
    uint8_t data_len;             /* 数据长度 */
    uint8_t data[FRAME_DATA_MAX]; /* 数据体 */
} Frame_t;

/* ============ 流式解析器 ============ */
typedef struct {
    uint8_t buf[FRAME_MAX_SIZE];
    uint16_t len;
} FrameParser_t;

/* ============ 解析结果 ============ */
typedef enum {
    FRAME_OK = 0,     /* 解析出一帧完整数据 */
    FRAME_INCOMPLETE, /* 数据不够，继续等 */
    FRAME_CRC_ERROR,  /* 校验失败，已自动重同步 */
    FRAME_INVALID,    /* 参数非法或无法恢复 */
} FrameStatus_t;

/* ============ 接口 ============ */
void FrameParser_Init(FrameParser_t *p);

/**
 * @brief  向解析器喂入数据并尝试解析一帧
 * @param  p     解析器句柄
 * @param  data  新数据（可为 NULL，只尝试从已有数据中解析下一帧）
 * @param  len   新数据长度
 * @param  out   解析结果输出
 * @retval 见 FrameStatus_t
 * @note   FRAME_OK 时，out 已填充，且已消耗的字节从内部缓冲移除
 *         调用者可以循环调用（data=NULL, len=0）来弹出剩余帧
 */
FrameStatus_t FrameParser_Feed(FrameParser_t *p,
                               const uint8_t *data, uint16_t len,
                               Frame_t *out);

/**
 * @brief  组帧（发送用）
 * @param  out        输出缓冲区
 * @param  buf_size   输出缓冲区大小
 * @param  seq        序号
 * @param  cmd        指令
 * @param  data       数据体（可为 NULL）
 * @param  data_len   数据长度（0~255）
 * @return 写入的字节数；失败返回 0
 */
uint16_t Frame_Build(uint8_t *out, uint16_t buf_size,
                     uint16_t seq, uint8_t cmd,
                     const uint8_t *data, uint8_t data_len);

/* 直接对一段 buffer 计算协议规定的 CRC32（取低16位） */
uint32_t Frame_CRC32(const uint8_t *data, uint32_t len);

void Frame_Send(BSP_UART_Bus_t bus, uint16_t seq, uint8_t cmd, uint8_t *data, uint8_t data_len, uint32_t timeout_ms);
#endif /* FRAME_H */