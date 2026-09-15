#ifndef BOOTLOADER_H
#define BOOTLOADER_H

#include <stdint.h>
#include <stdbool.h>
#include "ota_layout.h"

/* ============ 元信息结构（16字节） ============ */
typedef struct
{
    uint32_t magic;      /* 0x00: 魔数 OTA_MAGIC */
    uint32_t fw_size;    /* 0x04: 原始固件大小（未补齐） */
    uint32_t fw_crc32;   /* 0x08: 补齐后固件的CRC32 */
    uint32_t meta_crc32; /* 0x0C: 前12字节的CRC32 */
} __attribute__((packed)) OTA_Meta_t;

/* ============ Bootloader 入口（main 里调用） ============ */
void Bootloader_Run(void);

/* ============ 内部使用 ============ */
bool Bootloader_CheckMeta(OTA_Meta_t *meta);
bool Bootloader_VerifyFirmware(const OTA_Meta_t *meta);
bool Bootloader_LoadFirmware(const OTA_Meta_t *meta);
void Bootloader_InvalidateMeta(void);
bool Bootloader_IsAppValid(uint32_t app_addr);
void Bootloader_JumpToApp(uint32_t app_addr) __attribute__((noreturn));

/* 内部 Flash 操作 */
bool InternalFlash_Erase(uint32_t addr, uint32_t size);
bool InternalFlash_Write(uint32_t addr, const uint8_t *data, uint32_t len);

/* CRC32（与 APP、Python 端必须一致） */
uint32_t BL_CRC32(const uint8_t *data, uint32_t len);

#endif /* BOOTLOADER_H */