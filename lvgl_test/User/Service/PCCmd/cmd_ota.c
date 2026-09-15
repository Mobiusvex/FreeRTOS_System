

#include "cmd_ota.h"
#include "frame.h"
#include "frame_cmd.h"
#include "driver_w25q64.h"
#include "bsp_uart.h"
#include "debug_func.h"
#include "cmsis_os2.h"
#include <string.h>
#include "sys_data.h"
#include "user_sysDataStorageTask.h"

/* ============================================================
 *  常量定义
 * ============================================================ */
#define OTA_KEY_STREAM_LEN 240U    /* 密钥流长度 */
#define OTA_DATA_PER_PAGE 240U     /* 每页有效数据字节数 */
#define OTA_PAGE_SIZE 256U         /* W25Q64 页大小 */
#define OTA_RESERVED_SIZE 12U      /* 页内保留区大小 */
#define OTA_META_SIZE 16U          /* 元信息有效长度 */
#define OTA_META_SECTOR_SIZE 4096U /* W25Q64 扇区大小 */
#define OTA_PAGES_PER_SECTOR (OTA_META_SECTOR_SIZE / OTA_PAGE_SIZE)

#define OTA_CODE_MAX_SIZE 0X7A800U   /* 最大固件字节数 */
#define OTA_META_ADDR 0x000000U      /* 元信息扇区地址 */
#define OTA_DATA_BASE_ADDR 0x001000U /* 数据区起始地址 */

#define OTA_MAGIC 0x4F544131U   /* "OTA1" */
#define OTA_ACK_TIMEOUT_MS 100U /* ACK 发送超时 */

/* ============================================================
 *  密钥流（与 Python 端必须完全一致，240字节）
 *  Python: random.seed(2026); [random.randint(0,255) for _ in range(240)]
 * ============================================================ */

static const uint8_t KEY_STREAM[OTA_KEY_STREAM_LEN] = {
    0x8B, 0x47, 0x3F, 0x36, 0x3F, 0xD2, 0x95, 0x1D, 0xC4, 0xBF, 0x7B, 0xF2, 0x21, 0x9B, 0xF1, 0x4D,
    0x09, 0x7E, 0xED, 0xF6, 0xD7, 0xDC, 0x7D, 0x01, 0xEF, 0xCA, 0x06, 0x3C, 0xD3, 0x4B, 0x40, 0x6B,
    0xC9, 0xA7, 0x91, 0xBE, 0x52, 0x67, 0xB2, 0x3C, 0x4D, 0x23, 0x5E, 0x2B, 0xBC, 0xC4, 0x1F, 0x85,
    0x24, 0xCC, 0x72, 0x71, 0x01, 0x43, 0xB7, 0x6F, 0x26, 0x62, 0xEC, 0xB1, 0x20, 0xA3, 0x70, 0x8C,
    0x52, 0x02, 0x78, 0x83, 0x58, 0x3E, 0xCF, 0xE2, 0xB2, 0x88, 0x43, 0x17, 0x71, 0x29, 0xF0, 0x09,
    0xB4, 0x77, 0x0B, 0xCE, 0x6B, 0x27, 0x2A, 0x2A, 0x2E, 0xCD, 0x1D, 0xA0, 0xD5, 0x98, 0x22, 0x3C,
    0x85, 0xA6, 0x57, 0x7C, 0xC6, 0x3A, 0xB8, 0xE6, 0xD7, 0xC5, 0xFE, 0x26, 0x3F, 0x47, 0xC6, 0x5F,
    0x66, 0x9F, 0xA8, 0x2B, 0x2F, 0x95, 0xF8, 0x61, 0x6F, 0xF4, 0xA9, 0x16, 0xA6, 0x9E, 0x35, 0x16,
    0x79, 0xC5, 0xA3, 0xF8, 0xED, 0x8B, 0x13, 0x83, 0x95, 0x89, 0x37, 0x0F, 0xF3, 0x81, 0x6D, 0xA8,
    0xB7, 0xD2, 0x76, 0xE4, 0xF4, 0xC2, 0xE4, 0x71, 0xF0, 0xF1, 0x59, 0x0D, 0x34, 0x4A, 0xC5, 0x3F,
    0x1E, 0xFF, 0x24, 0xDB, 0xA4, 0xD1, 0x0C, 0x06, 0x87, 0xFB, 0xDC, 0x3B, 0x5E, 0xAF, 0xE1, 0xFB,
    0x44, 0x36, 0x67, 0x5E, 0x14, 0xC3, 0x2B, 0xF4, 0xE8, 0x9D, 0x4E, 0xFA, 0xE3, 0x63, 0x5F, 0xA1,
    0x1E, 0x29, 0xD5, 0x90, 0xD1, 0x2F, 0xA5, 0x73, 0x09, 0xCC, 0xD1, 0x2B, 0x4E, 0x2C, 0x09, 0x46,
    0x32, 0x6E, 0xB8, 0x1D, 0x68, 0x1C, 0x43, 0x2E, 0x2D, 0xEB, 0x4A, 0x5C, 0xFB, 0xB2, 0x8A, 0x4B,
    0xEA, 0x1F, 0xB3, 0x56, 0x4E, 0x1B, 0x95, 0x85, 0x2C, 0x37, 0x1A, 0xC9, 0xAC, 0x86, 0x1C, 0x07};

typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_START,
    OTA_STATE_OK
} OTA_State_t;
/* ============================================================
 *  OTA 上下文
 * ============================================================ */
typedef struct {
    OTA_State_t active;            /* 0:未开始，1:正在写入，2:写入完成 */
    uint16_t total_packets;        /* 总包数 */
    uint32_t firmware_size;        /* 原始固件字节数（补齐前） */
    uint16_t received_packets;     /* 已成功写入的包数 */
    uint16_t next_sector_to_erase; /* 下一个待擦除的扇区号 */
} OTA_Context_t;

static OTA_Context_t s_ota_ctx;

extern osThreadId_t user_sysDataStorageTaskHandle;

/* ============================================================
 *  工具：小端读写
 * ============================================================ */
static void put_u32_le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint16_t get_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t get_u32_le(const uint8_t *p) {
    return (uint32_t)p[0]
           | ((uint32_t)p[1] << 8)
           | ((uint32_t)p[2] << 16)
           | ((uint32_t)p[3] << 24);
}

/**
 * @brief 异或解密数据
 * @param data 要解密的数据
 * @param len 数据长度
 * @param abs_offset 该字节在整个固件中的绝对偏移
 * @retval void
 */
static void ota_decrypt(uint8_t *data, uint16_t len, uint32_t abs_offset) {
    for (uint16_t i = 0; i < len; i++) {
        data[i] ^= KEY_STREAM[(abs_offset + i) % OTA_KEY_STREAM_LEN];
    }
}

/**
 * @brief 发送 OTA ACK
 * @param seq 序号
 * @param ack ACK 码
 * @retval void
 */
static void ota_send_ack(uint16_t seq, uint8_t ack) {
    Frame_Send(BSP_UART_PC, seq, CMD_OTA_ACK, &ack, 1, OTA_ACK_TIMEOUT_MS);
}

/**
 * @brief 写入一页数据到 Flash
 *  页布局：[0..239]数据 | [240..243]页CRC32 | [244..255]保留0xFF
 * @param page_idx 页索引
 * @param data 要写入的数据
 * @param data_len 数据长度
 * @retval true 写入成功，false 写入失败
 */
static bool ota_write_data_page(uint16_t page_idx,
                                const uint8_t *data,
                                uint16_t data_len) {
    uint8_t page[OTA_PAGE_SIZE];

    /* 1. 拷贝有效数据 */
    memcpy(page, data, data_len);

    /* 2. 剩余部分填 0xFF（最后一包不足 240 时） */
    if (data_len < OTA_DATA_PER_PAGE) {
        memset(&page[data_len], 0xFF, OTA_DATA_PER_PAGE - data_len);
    }

    /* 3. 计算本页 CRC32（只对前 240 字节） */
    uint32_t page_crc = Frame_CRC32(page, OTA_DATA_PER_PAGE);
    put_u32_le(&page[OTA_DATA_PER_PAGE], page_crc);
    /* 4. 保留 12 字节填 0xFF */
    memset(&page[OTA_DATA_PER_PAGE + 4], 0xFF, OTA_RESERVED_SIZE);

    /* 5. 写入 Flash（地址天然 256 对齐，一次写满一页） */
    uint32_t addr = OTA_DATA_BASE_ADDR + (uint32_t)page_idx * OTA_PAGE_SIZE;
    return BSP_W25Qxx_PageWrite(page, addr, OTA_PAGE_SIZE);
}

/**
 * @brief 验证并计算总 CRC32
 * @param total_packets 总包数
 * @param out_crc 输出总 CRC32
 * @retval true 校验通过，false 校验失败
 */
static bool ota_verify_and_calc_crc(uint16_t total_packets, uint32_t *out_crc) {
    uint32_t crc = 0xFFFFFFFFU;
    uint8_t page[OTA_PAGE_SIZE];

    for (uint16_t i = 0; i < total_packets; i++) {
        /* 1. 读一整页 */
        uint32_t addr = OTA_DATA_BASE_ADDR + (uint32_t)i * OTA_PAGE_SIZE;
        if (!BSP_W25Qxx_BufferRead(page, addr, OTA_PAGE_SIZE)) {
            RTT_PRINTF("OTA Verify: read page %u failed\n", i);
            return false;
        }

        /* 2. 校验本页 CRC */
        uint32_t pc_calc = Frame_CRC32(page, OTA_DATA_PER_PAGE);
        uint32_t pc_stored = get_u32_le(&page[OTA_DATA_PER_PAGE]);
        if (pc_calc != pc_stored) {
            RTT_PRINTF("OTA Verify: page %u CRC err (calc=0x%08X stored=0x%08X)\n",
                       i, pc_calc, pc_stored);
            return false;
        }

        /* 3. 把 240 字节数据累积到总 CRC 里 */
        for (uint16_t k = 0; k < OTA_DATA_PER_PAGE; k++) {
            crc ^= page[k];
            for (int b = 0; b < 8; b++) {
                crc = (crc & 1) ? (0xEDB88320U ^ (crc >> 1)) : (crc >> 1);
            }
        }

        /* 4. 让出 CPU（防止饿死其他任务、避免看门狗复位） */
        if ((i & 0x1F) == 0x1F) {
            osDelay(1);
        }
    }

    *out_crc = crc ^ 0xFFFFFFFFU;
    return true;
}

/**
 * @brief 处理起始包（CMD 0x11）
 * @param frame 帧数据
 * @retval void
 */
void OTA_HandleStart(const Frame_t *frame) {
    /* 1. 数据长度校验 */
    if (frame->data_len != 6) {
        RTT_PRINTF("OTA Start: bad data_len=%u\n", frame->data_len);
        ota_send_ack(frame->seq, ACK_PARAM_ERROR);
        return;
    }

    /* 2. 解析 */
    uint16_t total = get_u16_le(&frame->data[0]);
    uint32_t fw_size = get_u32_le(&frame->data[2]);

    if (total == 0 || fw_size == 0 || fw_size > OTA_CODE_MAX_SIZE) {
        RTT_PRINTF("OTA Start: invalid params total=%u size=%u\n", total, fw_size);
        ota_send_ack(frame->seq, ACK_OTA_SIZE_ERROR);
        return;
    }

    /* 3. 包数合理性检查：total*240 必须 >= fw_size */
    if ((uint32_t)total * OTA_DATA_PER_PAGE < fw_size) {
        RTT_PRINTF("OTA Start: total_packets too small\n");
        ota_send_ack(frame->seq, ACK_PARAM_ERROR);
        return;
    }

    RTT_PRINTF("OTA Start: fw_size=%u, total_packets=%u\n", fw_size, total);

    /* 4. 初始化上下文 */
    s_ota_ctx.active = OTA_STATE_START;
    s_ota_ctx.total_packets = total;
    s_ota_ctx.firmware_size = fw_size;
    s_ota_ctx.received_packets = 0;
    s_ota_ctx.next_sector_to_erase = 0;

    RTT_PRINTF("OTA Start: erase done\n");
    ota_send_ack(frame->seq, ACK_OK);
}

/**
 * @brief 处理数据包（CMD 0x12)
 * @param frame 帧数据
 * @retval void
 */
void OTA_HandleData(const Frame_t *frame) {
    /* 1. 状态检查 */
    if (!s_ota_ctx.active) {
        RTT_PRINTF("OTA Data: not active, reject seq=%u\n", frame->seq);
        ota_send_ack(frame->seq, ACK_FAIL);
        return;
    }

    /* 2. 长度检查 */
    if (frame->data_len == 0 || frame->data_len > OTA_DATA_PER_PAGE) {
        RTT_PRINTF("OTA Data: bad data_len=%u\n", frame->data_len);
        ota_send_ack(frame->seq, ACK_PARAM_ERROR);
        return;
    }

    /* 3. 序号检查 */
    if (frame->seq >= s_ota_ctx.total_packets) {
        RTT_PRINTF("OTA Data: seq %u out of range\n", frame->seq);
        ota_send_ack(frame->seq, ACK_PARAM_ERROR);
        return;
    }

    /* ★ 4. 按需擦除：检查当前包所在扇区是否已擦过 */
    uint16_t sector_idx = frame->seq / OTA_PAGES_PER_SECTOR;

    if (sector_idx >= s_ota_ctx.next_sector_to_erase) {
        uint32_t sector_addr = OTA_DATA_BASE_ADDR
                               + (uint32_t)sector_idx * OTA_META_SECTOR_SIZE;

        RTT_PRINTF("OTA Data: erase sector %u (addr=0x%08X)\n",
                   sector_idx, sector_addr);

        if (!BSP_W25Qxx_SectorErase(sector_addr)) {
            RTT_PRINTF("OTA Data: erase sector %u failed\n", sector_idx);
            ota_send_ack(frame->seq, ACK_FLASH_ERROR);
            return;
        }

        s_ota_ctx.next_sector_to_erase = sector_idx + 1;

        /* 让出 CPU，避免看门狗复位、避免饿死其他任务 */
        osDelay(1);
    }

    /* 5. 拷贝到本地缓冲 */
    uint8_t data[OTA_DATA_PER_PAGE];
    memcpy(data, frame->data, frame->data_len);

    /* 6. 解密 */
    uint32_t abs_offset = (uint32_t)frame->seq * OTA_DATA_PER_PAGE;
    ota_decrypt(data, frame->data_len, abs_offset);

    /* 7. 写入 Flash 页 */
    if (!ota_write_data_page(frame->seq, data, frame->data_len)) {
        RTT_PRINTF("OTA Data: write page %u failed\n", frame->seq);
        ota_send_ack(frame->seq, ACK_FLASH_ERROR);
        return;
    }

    /* 8. 进度更新 */
    s_ota_ctx.received_packets++;
    if ((s_ota_ctx.received_packets % 50) == 0) {
        RTT_PRINTF("OTA progress: %u/%u\n",
                   s_ota_ctx.received_packets, s_ota_ctx.total_packets);
    }

    /* 9. 回复 ACK 成功 */
    ota_send_ack(frame->seq, ACK_OK);
}

/**
 * @brief 处理结束包（CMD 0x14）
 * @param frame 帧数据
 * @retval void
 */
void OTA_HandleEnd(const Frame_t *frame) {
    /* 1. 状态检查 */
    if (!s_ota_ctx.active) {
        ota_send_ack(frame->seq, ACK_FAIL);
        return;
    }

    /* 2. 长度检查 */
    if (frame->data_len != 4) {
        RTT_PRINTF("OTA End: bad data_len=%u\n", frame->data_len);
        ota_send_ack(frame->seq, ACK_PARAM_ERROR);
        return;
    }

    /* 3. 序号检查：应等于总包数 */
    if (frame->seq != s_ota_ctx.total_packets) {
        RTT_PRINTF("OTA End: seq %u != total %u\n",
                   frame->seq, s_ota_ctx.total_packets);
        ota_send_ack(frame->seq, ACK_PARAM_ERROR);
        return;
    }

    /* 4. 完整性检查：所有包都必须成功写入 */
    if (s_ota_ctx.received_packets != s_ota_ctx.total_packets) {
        RTT_PRINTF("OTA End: only %u/%u packets received\n",
                   s_ota_ctx.received_packets, s_ota_ctx.total_packets);
        ota_send_ack(frame->seq, ACK_FRAME_LACK);
        s_ota_ctx.active = OTA_STATE_IDLE;
        return;
    }

    /* 5. 解析上位机发来的总 CRC32 */
    uint32_t recv_crc = get_u32_le(frame->data);

    /* 6. 从 Flash 读回全量数据，逐页校验 + 计算总 CRC */
    uint32_t calc_crc;
    if (!ota_verify_and_calc_crc(s_ota_ctx.total_packets, &calc_crc)) {
        RTT_PRINTF("OTA End: verify failed\n");
        ota_send_ack(frame->seq, ACK_PAGE_CRC_ERROR);
        s_ota_ctx.active = OTA_STATE_IDLE;
        return;
    }

    RTT_PRINTF("OTA End: recv_crc=0x%08X calc_crc=0x%08X\n", recv_crc, calc_crc);

    /* 7. 比对 */
    if (calc_crc != recv_crc) {
        RTT_PRINTF("OTA End: CRC mismatch!\n");
        ota_send_ack(frame->seq, ACK_PAKET_CRC_ERROR);
        s_ota_ctx.active = OTA_STATE_IDLE;
        return;
    }

    /* 8. ★ 校验通过，写元信息扇区
     * 布局：[魔数(4) | 固件大小(4) | 总CRC(4) | 元信息CRC(4)]
     * 元信息CRC 是对前 12 字节算的，防止元信息自身损坏
     */
    uint8_t meta[OTA_META_SIZE];
    memset(meta, 0xFF, sizeof(meta));
    put_u32_le(&meta[0x00], OTA_MAGIC);
    put_u32_le(&meta[0x04], s_ota_ctx.firmware_size);
    put_u32_le(&meta[0x08], recv_crc);
    uint32_t meta_crc = Frame_CRC32(meta, 12);
    put_u32_le(&meta[0x0C], meta_crc);

    /* 擦除元信息扇区 */
    if (!BSP_W25Qxx_SectorErase(OTA_META_ADDR)) {
        RTT_PRINTF("OTA End: erase meta sector failed\n");
        ota_send_ack(frame->seq, ACK_FLASH_ERROR);
        s_ota_ctx.active = OTA_STATE_IDLE;
        return;
    }

    /* 写入元信息 */
    if (!BSP_W25Qxx_BufferWrite(meta, OTA_META_ADDR, sizeof(meta))) {
        RTT_PRINTF("OTA End: write meta failed\n");
        ota_send_ack(frame->seq, ACK_FLASH_ERROR);
        s_ota_ctx.active = OTA_STATE_IDLE;
        return;
    }

    /* 9. 全部完成 */
    RTT_PRINTF("OTA End: complete! meta written.\n");
    s_ota_ctx.active = OTA_STATE_OK;
    ota_send_ack(frame->seq, ACK_OK);

    /* ★ 可选：设置升级标志 + 复位，让 Bootloader 在下一轮启动时校验并跳转 */
    /* OTA_SetBootFlag();
       NVIC_SystemReset(); */
}
extern osMessageQueueId_t xCmdDisplayQueue;

/**
 * @brief OTA 系统数据更新
 * @retval true 更新成功，false 更新失败
 * @retval
 * @note 状态改变立即更新，数据包进度条30包更新一次
 */
bool ota_sys_data_update(void) {
    static uint8_t index = 0;
    static OTA_State_t last_state = OTA_STATE_IDLE;
    SYS_DataEventType_t event;
    bool update = false;
    bool ret = false;

    if (last_state != s_ota_ctx.active) {
        update = true;
        index = 0;
    }
    if ((index++) == 30) {
        update = true;
        index = 0;
    }
    if (update) {
        SYS_DATA_SetPaketUpdateData(s_ota_ctx.active, s_ota_ctx.received_packets * 100 / s_ota_ctx.total_packets);
        event = SYS_PAKET_UPDATE; // 更新OTA显示
        osMessageQueuePut(xCmdDisplayQueue, &event, 0, 50);
        ret = true;
    }
    if (s_ota_ctx.active == OTA_STATE_OK) {
        SYS_DATA_SetNewPaketState(SYS_NEW_PAKET_READY);
        osThreadFlagsSet(user_sysDataStorageTaskHandle, FLAG_MSG_DATA_STORAGE);
    }
    last_state = s_ota_ctx.active;
    return ret;
}

/**
 * @brief OTA 显示清理
 * @retval void
 */
void ota_display_clean() {
    SYS_DataEventType_t event;
    SYS_DATA_SetPaketUpdateData(0, 0);
    s_ota_ctx.active = OTA_STATE_IDLE;
    event = SYS_PAKET_UPDATE; // 更新OTA显示
    osMessageQueuePut(xCmdDisplayQueue, &event, 0, 50);
}