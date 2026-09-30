#include "frame_cmd.h"
#include "frame.h"
#include "driver_w25q64.h"
#include "bsp_uart.h"
#include "debug_func.h"
#include <string.h>
#include <stdint.h>
#include "tools_func.h"
#include "cmd_audio.h"
#include "HWDataAccess.h"

/* ============ 信息页格式（32字节） ============ */
/*
 * [0..3]   魔数  "VOIC" = 0x564F4943
 * [4..7]   文件大小（4字节小端）
 * [8..27]  文件名（20字节，末尾'\0'）
 * [28..31] 前28字节的CRC32（4字节小端）
 */
#define AUDIO_META_MAGIC 0x564F4943U
#define AUDIO_META_SIZE 32U
#define AUDIO_META_NAME_LEN 20U

#define FLASH_PAGE_SIZE 256U
#define AUDIO_SECTOR_SIZE 4096U
#define AUDIO_PAGES_PER_SECTOR (AUDIO_SECTOR_SIZE / FLASH_PAGE_SIZE) /* = 16 */
#define AUDIO_ACK_TIMEOUT_MS 100U

#define AUDIO_MAX_PACK_DATA 240U

/* ============================================================
 *  上下文
 * ============================================================ */
typedef struct {
    bool active;
    enum_slot_t slot;
    uint16_t total_packets;
    uint32_t file_size;
    char filename[AUDIO_META_NAME_LEN];

    /* ★ 新增：页缓冲区 */
    uint8_t page_buf[FLASH_PAGE_SIZE]; /* 256 字节拼接缓冲 */
    uint16_t page_buf_len;             /* 当前缓冲的字节数（0~255） */
    uint32_t write_offset;             /* 已写入Flash的字节数（256对齐） */
    uint16_t next_sector_to_erase;
    uint16_t received_packets;
} AUDIO_Context_t;

static AUDIO_Context_t s_audio_ctx;

/**
 * @brief 发送 ACK
 * @param seq 序号
 * @param ack ACK 码
 * @retval void
 */
static void audio_send_ack(uint16_t seq, uint8_t ack) {
    Frame_Send(BSP_UART_PC, seq, CMD_AUDIO_ACK, &ack, 1, AUDIO_ACK_TIMEOUT_MS);
}

/**
 * @brief 确保写入范围 [offset, offset+len) 所在的扇区都已擦除
 * @param offset 起始偏移
 * @param len 长度
 * @retval true 成功
 * @retval false 失败
 */
static bool audio_ensure_erased(uint32_t offset, uint32_t len) {
    if (len == 0) return true;

    uint16_t last_sector = (uint16_t)((offset + len - 1) / AUDIO_SECTOR_SIZE);

    while (s_audio_ctx.next_sector_to_erase <= last_sector) {
        uint32_t addr = AUDIO_SLOT_BASE(s_audio_ctx.slot)
                        + (uint32_t)s_audio_ctx.next_sector_to_erase * AUDIO_SECTOR_SIZE;

        RTT_PRINTF("Audio: erase sector %u @0x%08X\n",
                   s_audio_ctx.next_sector_to_erase, addr);

        if (!BSP_W25Qxx_SectorErase(addr)) {
            return false;
        }
        s_audio_ctx.next_sector_to_erase++;
        osDelay(1);
    }

    return true;
}

/**
 * @brief 把 page_buf 里的 256 字节写入 Flash 并复位缓冲
 * @retval true 成功
 */
static bool audio_flush_page(void) {
    /* 确保目标扇区已擦除 */
    if (!audio_ensure_erased(s_audio_ctx.write_offset, FLASH_PAGE_SIZE)) {
        return false;
    }

    uint32_t addr = AUDIO_SLOT_BASE(s_audio_ctx.slot) + s_audio_ctx.write_offset;

    if (!BSP_W25Qxx_PageWrite(s_audio_ctx.page_buf, addr, FLASH_PAGE_SIZE)) {
        RTT_PRINTF("Audio: write page @0x%08X failed\n", addr);
        return false;
    }

    s_audio_ctx.write_offset += FLASH_PAGE_SIZE;
    s_audio_ctx.page_buf_len = 0;

    return true;
}

/**
 * @brief 处理起始包（CMD 0x21）
 * @param frame 帧
 * @retval void
 */
void AUDIO_HandleStart(const Frame_t *frame) {
    /* 1. 序号检查：1~5 */
    if (frame->seq < 1 || frame->seq > AUDIO_SLOT_COUNT) {
        RTT_PRINTF("Audio Start: invalid slot %u\n", frame->seq);
        audio_send_ack(frame->seq, ACK_FAIL);
        return;
    }

    /* 2. 数据长度检查：6（头）+ 2~20（文件名）= 8~26 */
    if (frame->data_len < 8 || frame->data_len > (6 + AUDIO_META_NAME_LEN)) {
        RTT_PRINTF("Audio Start: bad data_len %u\n", frame->data_len);
        audio_send_ack(frame->seq, ACK_FAIL);
        return;
    }

    /* 3. 解析 */
    uint16_t total = get_u16_le(&frame->data[0]); /* ★ 新增 */
    uint32_t file_size = get_u32_le(&frame->data[2]);
    uint8_t name_len = frame->data_len - 6;

    /* 4. 参数合法性检查 */
    if (total == 0 || file_size == 0 || file_size > AUDIO_DATA_SIZE) {
        RTT_PRINTF("Audio Start: file too large: %u\n", file_size);
        audio_send_ack(frame->seq, ACK_FAIL);
        return;
    }

    if ((uint32_t)total * AUDIO_MAX_PACK_DATA < file_size) {
        RTT_PRINTF("Audio Start: total_packets too small\n");
        audio_send_ack(frame->seq, ACK_PARAM_ERROR);
        return;
    }

    /* ★ 校验上位机给的总包数和实际计算的是否一致 */
    uint16_t calc_packets = (file_size + AUDIO_MAX_PACK_DATA - 1) / AUDIO_MAX_PACK_DATA;
    if (total != calc_packets) {
        RTT_PRINTF("Audio Start: total_packets mismatch, recv=%u calc=%u\n",
                   total, calc_packets);
        audio_send_ack(frame->seq, ACK_FAIL);
        return;
    }

    /* 5. 初始化上下文 */
    s_audio_ctx.active = true;
    s_audio_ctx.slot = (uint8_t)frame->seq;
    s_audio_ctx.total_packets = total;
    s_audio_ctx.file_size = file_size;
    s_audio_ctx.received_packets = 0;
    s_audio_ctx.page_buf_len = 0;
    s_audio_ctx.write_offset = 0;
    s_audio_ctx.next_sector_to_erase = 0;

    memset(s_audio_ctx.filename, 0, AUDIO_META_NAME_LEN);
    memcpy(s_audio_ctx.filename, &frame->data[6],
           (name_len <= AUDIO_META_NAME_LEN) ? name_len : AUDIO_META_NAME_LEN);
    s_audio_ctx.filename[AUDIO_META_NAME_LEN - 1] = '\0';

    RTT_PRINTF("Audio Start: slot=%u, size=%u, packets=%u, name='%s'\n",
               frame->seq, file_size, total, s_audio_ctx.filename);

    audio_send_ack(frame->seq, ACK_OK);
    HW_Interface.AUDIO.PlayVoicePut(AUDIO_SLOT_NONE); // 通知播放器停止播放
}

/**
 * @brief 处理数据包（CMD 0x22），每包最多 240 字节；累积到 256 后写一页
 * @param frame 帧
 * @retval void
 */
void AUDIO_HandleData(const Frame_t *frame) {
    if (!s_audio_ctx.active) {
        RTT_PRINTF("Audio Data: not active, seq=%u\n", frame->seq);
        audio_send_ack(frame->seq, ACK_FAIL);
        return;
    }

    /* 长度检查 */
    uint16_t data_len = frame->data_len;
    if (data_len == 0 || data_len > AUDIO_MAX_PACK_DATA) {
        RTT_PRINTF("Audio Data: bad data_len %u\n", data_len);
        audio_send_ack(frame->seq, ACK_FAIL);
        return;
    }

    /* 序号检查 */
    if (frame->seq >= s_audio_ctx.total_packets) {
        RTT_PRINTF("Audio Data: seq %u out of range\n", frame->seq);
        audio_send_ack(frame->seq, ACK_FAIL);
        return;
    }

    /* ★ 把新数据追加到 page_buf 末尾 */
    uint16_t src_offset = 0;
    uint16_t remain = data_len;

    while (remain > 0) {
        /* 计算 page_buf 里还能放多少 */
        uint16_t space = FLASH_PAGE_SIZE - s_audio_ctx.page_buf_len;
        uint16_t copy = (remain < space) ? remain : space;

        memcpy(&s_audio_ctx.page_buf[s_audio_ctx.page_buf_len],
               &frame->data[src_offset], copy);

        s_audio_ctx.page_buf_len += copy;
        src_offset += copy;
        remain -= copy;

        /* 缓冲满 256 → 写一页 */
        if (s_audio_ctx.page_buf_len == FLASH_PAGE_SIZE) {
            if (!audio_flush_page()) {
                audio_send_ack(frame->seq, ACK_FAIL);
                return;
            }
        }
    }

    /* 更新进度 */
    s_audio_ctx.received_packets++;
    if ((s_audio_ctx.received_packets % 50) == 0) {
        RTT_PRINTF("Audio: %u/%u packets, written %u bytes\n",
                   s_audio_ctx.received_packets,
                   s_audio_ctx.total_packets,
                   s_audio_ctx.write_offset + s_audio_ctx.page_buf_len);
    }

    audio_send_ack(frame->seq, ACK_OK);
}

/**
 * @brief 处理结束包（CMD 0x24）
 * @param frame 帧
 * @retval void
 */
void AUDIO_HandleEnd(const Frame_t *frame) {
    if (!s_audio_ctx.active) {
        audio_send_ack(frame->seq, ACK_FAIL);
        return;
    }

    if (frame->seq != s_audio_ctx.total_packets) {
        RTT_PRINTF("Audio End: seq %u != total %u\n",
                   frame->seq, s_audio_ctx.total_packets);
        audio_send_ack(frame->seq, ACK_FAIL);
        s_audio_ctx.active = false;
        return;
    }

    if (s_audio_ctx.received_packets != s_audio_ctx.total_packets) {
        RTT_PRINTF("Audio End: only %u/%u packets\n",
                   s_audio_ctx.received_packets, s_audio_ctx.total_packets);
        audio_send_ack(frame->seq, ACK_FAIL);
        s_audio_ctx.active = false;
        return;
    }

    /* ★ 刷最后一页（如果 page_buf 还有残留） */
    if (s_audio_ctx.page_buf_len > 0) {
        /* 用 0xFF 补齐到 256 字节 */
        memset(&s_audio_ctx.page_buf[s_audio_ctx.page_buf_len],
               0xFF, FLASH_PAGE_SIZE - s_audio_ctx.page_buf_len);
        s_audio_ctx.page_buf_len = FLASH_PAGE_SIZE;

        RTT_PRINTF("Audio: flushing last page (%u bytes real)\n",
                   s_audio_ctx.file_size - s_audio_ctx.write_offset);

        if (!audio_flush_page()) {
            audio_send_ack(frame->seq, ACK_FAIL);
            s_audio_ctx.active = false;
            return;
        }
    }

    /* 校验：实际写入字节数是否 >= file_size */
    if (s_audio_ctx.write_offset < s_audio_ctx.file_size) {
        RTT_PRINTF("Audio End: written %u < file_size %u\n",
                   s_audio_ctx.write_offset, s_audio_ctx.file_size);
        audio_send_ack(frame->seq, ACK_FAIL);
        s_audio_ctx.active = false;
        return;
    }

    /* ========== 组装信息页 ========== */
    uint8_t meta[AUDIO_META_SIZE];
    memset(meta, 0xFF, sizeof(meta));
    put_u32_le(&meta[0], AUDIO_META_MAGIC);
    put_u32_le(&meta[4], s_audio_ctx.file_size);
    memcpy(&meta[8], s_audio_ctx.filename, AUDIO_META_NAME_LEN);
    put_u32_le(&meta[28], Frame_CRC32(meta, 28));

    /* 擦除信息页扇区 */
    uint32_t meta_addr = AUDIO_META_ADDR(s_audio_ctx.slot);
    RTT_PRINTF("Audio End: erasing meta @0x%08X\n", meta_addr);

    if (!BSP_W25Qxx_SectorErase(meta_addr)) {
        audio_send_ack(frame->seq, ACK_FAIL);
        s_audio_ctx.active = false;
        return;
    }

    if (!BSP_W25Qxx_BufferWrite(meta, meta_addr, sizeof(meta))) {
        audio_send_ack(frame->seq, ACK_FAIL);
        s_audio_ctx.active = false;
        return;
    }

    RTT_PRINTF("Audio End: complete! slot=%u, name='%s', size=%u, flash=%u\n",
               s_audio_ctx.slot, s_audio_ctx.filename,
               s_audio_ctx.file_size, s_audio_ctx.write_offset);
    s_audio_ctx.active = false;
    audio_send_ack(frame->seq, ACK_OK);
}

/**
 * @brief 获取音频文件信息
 * @param slot 音频槽号
 * @param filename 文件名
 * @param file_size 文件大小
 * @retval true 成功，false 失败
 */
bool AUDIO_GetInfo(enum_slot_t slot, char *filename, uint32_t *file_size) {
    if (slot < 1 || slot > AUDIO_SLOT_COUNT) return false;

    uint8_t meta[AUDIO_META_SIZE];
    if (!BSP_W25Qxx_BufferRead(meta, AUDIO_META_ADDR(slot), sizeof(meta))) {
        return false;
    }
    if (get_u32_le(&meta[0]) != AUDIO_META_MAGIC) return false;

    uint32_t calc = Frame_CRC32(meta, 28);
    uint32_t recv = get_u32_le(&meta[28]);
    if (calc != recv) {
        RTT_PRINTF("Audio: slot %u meta CRC err\n", slot);
        return false;
    }

    if (filename) {
        memcpy(filename, &meta[8], AUDIO_META_NAME_LEN);
        filename[AUDIO_META_NAME_LEN - 1] = '\0';
    }
    if (file_size) {
        *file_size = get_u32_le(&meta[4]);
    }
    return true;
}

/**
 * @brief 读取音频文件数据
 * @param slot 音频槽号
 * @param offset 偏移
 * @param buf 缓冲区
 * @param len 长度
 * @retval true 成功，false 失败
 */
bool AUDIO_ReadData(enum_slot_t slot, uint32_t offset, uint8_t *buf, uint32_t len) {
    if (slot < 1 || slot > AUDIO_SLOT_COUNT) return false;
    if (offset + len > AUDIO_DATA_SIZE) return false;

    uint32_t addr = AUDIO_SLOT_BASE(slot) + offset;
    return BSP_W25Qxx_BufferRead(buf, addr, (uint16_t)len);
}

void AUDIO_HandlePlay(const Frame_t *frame) {
    if (frame->data[0] < 1 || frame->data[0] > AUDIO_SLOT_COUNT) {
        RTT_PRINTF("Audio Start: invalid slot %u\n", frame->seq);
        audio_send_ack(frame->seq, ACK_FAIL);
        return;
    }
    HW_Interface.AUDIO.PlayVoicePut(frame->data[0]);
    audio_send_ack(frame->seq, ACK_OK);
}