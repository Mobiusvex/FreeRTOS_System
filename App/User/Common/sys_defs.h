#ifndef SYS_DEFS_H
#define SYS_DEFS_H
#include "stdint.h"
typedef enum {
    SYS_OK = 0,
    SYS_ERROR = 1,
    SYS_TIMEOUT = 2,
    SYS_BUSY = 3,
    SYS_NO_ACK = 4,
    SYS_INVALID_PARAM = 5,
    SYS_INVALID_DATA = 6
} SYS_StatusTypeDef;

#define FLASH_PAGE_SIZE 256U    /* W25Q64 页大小 */
#define FLASH_SECTOR_SIZE 4096U /* W25Q64 扇区大小 */
#define FLASH_PAGES_PER_SECTOR (FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE)

/* ============ 外部FLASH分区（W25Q64，8MB） ============ */
/*
 * OTA 使用（前 1MB）:0x000000 ~ 0x0FFFFF
 *
 * 语音文件使用 slot1（1MB-2MB):0X100000~0X1FFFFF
 * 语音文件使用 slot2（2MB-3MB):0X200000~0X2FFFFF
 * 语音文件使用 slot3（3MB-4MB):0X300000~0X3FFFFF
 * 语音文件使用 slot4（4MB-5MB):0X400000~0X4FFFFF
 * 语音文件使用 slot5（5MB-6MB):0X500000~0X5FFFFF
 * 每个 slot 结构：
 *   数据区:  [slot*0x100000U, slot*0x100000U + 1MB - 4KB)   最大 1MB-4KB
 *   信息页:  [slot*0x100000U + 1MB - 4KB, [slot*0x100000U + 1MB)   4KB
 */

/* ============================ OTA固件元信息扇区地址 =============================== */
#define OTA_META_ADDR 0x000000U /* OTA固件元信息扇区地址 */
/* ============ OTA信息页格式（16字节） ============ */
/*
 * [0..3]   魔数  "OTA" = 0x4F544131U
 * [4..7]   文件大小（4字节小端）
 * [8..11] 固件包所有字节的CRC32（4字节小端）
 * [12..15]OTA信息页前12字节的CRC32（4字节小端）
 */
#define OTA_META_SIZE 16U     /* 元信息有效长度 */
#define OTA_MAGIC 0x4F544131U /* "OTA" */

/* ============ OTA数据区起始地址 ============ */
#define OTA_DATA_BASE_ADDR 0x001000U
/* ============ OTA固件数据页格式（256字节一页） ============ */
/*
 * [0..239]     数据区（240字节）
 * [240..243]  CRC32区（34字节）
 * [244..255]  页内保留区(12字节)
 */
#define OTA_KEY_STREAM_LEN 240U /* 密钥流长度 */
#define OTA_DATA_PER_PAGE 240U  /* 每页有效数据字节数 */
#define OTA_RESERVED_SIZE 12U   /* 页内保留区大小 */

/* ============================== 语音文件分区 ======================================*/
/* 语音文件 slot 数量 */
#define AUDIO_SLOT_COUNT 5U

/* 每个 slot 的基地址（数据区起始） */
#define AUDIO_SLOT_BASE(n) (0x100000U + ((uint32_t)((n) - 1) * 0x100000U))

/* 数据区大小：1MB - 4KB = 1044480 字节 */
#define AUDIO_DATA_SIZE (0x100000U - 0x1000U)

/* 信息页地址（每 slot 末尾的 4KB 扇区） */
#define AUDIO_META_ADDR(n) (AUDIO_SLOT_BASE(n) + AUDIO_DATA_SIZE)

/* ============ AUDIO信息页格式（32字节） ============ */
/*
 * [0..3]   魔数  "VOICE" = 0x564F4943
 * [4..7]   文件大小（4字节小端）
 * [8..27]  文件名（20字节，末尾'\0'）
 * [28..31] 前28字节的CRC32（4字节小端）
 */
#define AUDIO_META_MAGIC 0x564F4943U
#define AUDIO_META_SIZE 32U
#define AUDIO_META_NAME_LEN 20U

#define EXTRAM_SECTION __attribute__((section(".extram"), aligned(4)))
#endif
