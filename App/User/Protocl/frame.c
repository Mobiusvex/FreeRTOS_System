#include "frame.h"
#include <string.h>

/* ============================================================
 *  CRC32（标准反射算法，与 Python binascii.crc32 结果一致）
 *  多项式: 0xEDB88320，初值 0xFFFFFFFF，输出异或 0xFFFFFFFF
 * ============================================================ */
uint32_t Frame_CRC32(const uint8_t *data, uint32_t len) {
    uint32_t crc = 0xFFFFFFFFU;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int k = 0; k < 8; k++) {
            crc = (crc & 1) ? (0xEDB88320U ^ (crc >> 1)) : (crc >> 1);
        }
    }
    return crc ^ 0xFFFFFFFFU;
}

/* ============================================================
 *  初始化
 * ============================================================ */
void FrameParser_Init(FrameParser_t *p) {
    p->len = 0;
}

/* ============================================================
 *  从缓冲区头部丢弃 n 字节（保留尾部）
 * ============================================================ */
static void parser_discard(FrameParser_t *p, uint16_t n) {
    if (n >= p->len) {
        p->len = 0;
        return;
    }
    memmove(p->buf, &p->buf[n], p->len - n);
    p->len -= n;
}

/* ============================================================
 *  尝试从内部缓冲区解析一帧
 * ============================================================ */
static FrameStatus_t frame_parse_one(FrameParser_t *p, Frame_t *out) {
    while (p->len > 0) {
        /* ---- 1. 找帧头 0x68 ---- */
        uint16_t head_pos = 0;
        while (head_pos < p->len && p->buf[head_pos] != FRAME_HEAD) {
            head_pos++;
        }
        if (head_pos >= p->len) {
            /* 整段无帧头，丢弃全部，等待新数据 */
            p->len = 0;
            return FRAME_INCOMPLETE;
        }
        /* 丢弃帧头之前的数据 */
        if (head_pos > 0) {
            parser_discard(p, head_pos);
        }

        /* ---- 2. 检查头部是否完整 ---- */
        if (p->len < FRAME_HEAD_LEN) {
            return FRAME_INCOMPLETE; /* 等更多数据 */
        }

        uint8_t data_len = p->buf[4];
        uint16_t frame_size = FRAME_HEAD_LEN + data_len + FRAME_CRC_LEN + FRAME_TAIL_LEN;

        /* ---- 3. 检查整帧是否完整 ---- */
        if (p->len < frame_size) {
            return FRAME_INCOMPLETE; /* 等更多数据 */
        }

        /* ---- 4. 检查尾部 0x86 ---- */
        if (p->buf[frame_size - 1] != FRAME_TAIL) {
            /* 尾部不对，说明这个 0x68 是数据里的巧合，丢弃一个字节重找 */
            parser_discard(p, 1);
            continue; /* 继续循环，尝试下一个位置 */
        }

        /* ---- 5. 校验 CRC（范围：从起始到数据结束）---- */
        uint32_t calc_crc = Frame_CRC32(p->buf, FRAME_HEAD_LEN + data_len);
        uint16_t recv_crc = (uint16_t)p->buf[FRAME_HEAD_LEN + data_len]
                            | ((uint16_t)p->buf[FRAME_HEAD_LEN + data_len + 1] << 8);

        if ((calc_crc & 0xFFFFU) != recv_crc) {
            /* CRC 失败：丢弃一个字节重同步 */
            parser_discard(p, 1);
            return FRAME_CRC_ERROR; /* 通知调用者发生了错误 */
        }

        /* ---- 6. 解析成功，填充输出 ---- */
        out->seq = (uint16_t)p->buf[1] | ((uint16_t)p->buf[2] << 8);
        out->cmd = p->buf[3];
        out->data_len = data_len;
        if (data_len > 0) {
            memcpy(out->data, &p->buf[FRAME_HEAD_LEN], data_len);
        }

        /* ---- 7. 消耗已解析的字节 ---- */
        parser_discard(p, frame_size);

        return FRAME_OK;
    }
    return FRAME_INCOMPLETE;
}

/* ============================================================
 *  喂入数据 + 尝试解析
 * ============================================================ */
FrameStatus_t FrameParser_Feed(FrameParser_t *p,
                               const uint8_t *data, uint16_t len,
                               Frame_t *out) {
    /* ---- 1. 追加新数据 ---- */
    if (data != NULL && len > 0) {
        /* 缓冲区剩余空间不够时，优先保留新数据（假设旧数据有问题） */
        if (p->len + len > FRAME_MAX_SIZE) {
            /* 空间不够，丢弃全部旧的，避免内存溢出 */
            p->len = 0;
        }
        uint16_t copy_len = len;
        if (copy_len > FRAME_MAX_SIZE - p->len) {
            copy_len = FRAME_MAX_SIZE - p->len;
        }
        memcpy(&p->buf[p->len], data, copy_len);
        p->len += copy_len;
    }

    /* ---- 2. 尝试解析 ---- */
    return frame_parse_one(p, out);
}

/* ============================================================
 *  组帧
 * ============================================================ */
uint16_t Frame_Build(uint8_t *out, uint16_t buf_size,
                     uint16_t seq, uint8_t cmd,
                     const uint8_t *data, uint8_t data_len) {
    uint16_t frame_size = FRAME_HEAD_LEN + data_len + FRAME_CRC_LEN + FRAME_TAIL_LEN;

    if (out == NULL || buf_size < frame_size) return 0;
    if (data_len > 0 && data == NULL) return 0;

    /* 头部 */
    out[0] = FRAME_HEAD;
    out[1] = (uint8_t)(seq & 0xFF); /* 序号小端 */
    out[2] = (uint8_t)((seq >> 8) & 0xFF);
    out[3] = cmd;
    out[4] = data_len;

    /* 数据体 */
    if (data_len > 0) {
        memcpy(&out[FRAME_HEAD_LEN], data, data_len);
    }

    /* CRC（低16位小端）*/
    uint32_t crc = Frame_CRC32(out, FRAME_HEAD_LEN + data_len);
    out[FRAME_HEAD_LEN + data_len] = (uint8_t)(crc & 0xFF);
    out[FRAME_HEAD_LEN + data_len + 1] = (uint8_t)((crc >> 8) & 0xFF);

    /* 尾部 */
    out[frame_size - 1] = FRAME_TAIL;

    return frame_size;
}

void Frame_Send(BSP_UART_Bus_t bus, uint16_t seq, uint8_t cmd, uint8_t *data, uint8_t data_len, uint32_t timeout_ms) {
    uint8_t frame[FRAME_MAX_SIZE];
    uint16_t frame_size = Frame_Build(frame, FRAME_MAX_SIZE, seq, cmd, data, data_len);
    BSP_UART_Transmit_Block(bus, frame, frame_size, timeout_ms);
}