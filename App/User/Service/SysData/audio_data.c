#include "audio_data.h"
#include "cmsis_os2.h"
#include "driver_w25q64.h"
#include <string.h>
#include "sys_defs.h"
#include "tools_func.h"
#include "debug_func.h"

/* 数组彻底私有 */
static char s_name[AUDIO_SLOT_COUNT][AUDIO_META_NAME_LEN];

/**
 * @brief 从外部flash加载所有音频文件名字到数组
 */
void AudioNames_LoadAll(void) {
    for (int i = 1; i <= AUDIO_SLOT_COUNT; i++) {
        AudioNames_LoadOne(i);
    }
}

/**
 * @brief 从外部flash加载单个音频文件名字到数组
 * @param slot 音频槽号
 */
void AudioNames_LoadOne(enum_slot_t slot) {
    if (slot < 1 || slot > AUDIO_SLOT_COUNT) return;
    char filename[AUDIO_META_NAME_LEN] = {0};
    uint32_t file_size;
    if (!AUDIO_GetInfo(slot, filename, &file_size)) {
        strcpy(filename, "(error)");
    }

    uint32_t lock_state = osKernelLock();
    memcpy(s_name[slot - 1], filename, AUDIO_META_NAME_LEN);
    osKernelRestoreLock(lock_state);
}

/**
 * @brief 获取音频文件名字
 * @param slot 音频槽号
 * @param out 输出缓冲区
 * @param out_size 输出缓冲区大小
 * @retval true 成功，false 失败
 */
bool AudioNames_Get(int slot, char *out, uint8_t out_size) {
    if (!out || out_size == 0) return false;

    if (slot < 1 || slot > AUDIO_SLOT_COUNT) {
        out[0] = '\0';
        return false;
    }

    uint32_t lock_state = osKernelLock();
    size_t n = AUDIO_META_NAME_LEN;
    if (n >= out_size) n = out_size - 1;
    memcpy(out, s_name[slot - 1], n);
    out[n] = '\0';
    osKernelRestoreLock(lock_state);

    return true;
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

    uint32_t calc = Soft_CRC32(meta, 28);
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
