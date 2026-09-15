// sys_storage.c
#include "sys_storage.h"
#include "bsp_flash.h"
#include <string.h>
#include "stdbool.h"
#include "sys_crc.h"

// sys_storage.c
#define STORAGE_SIZE 512                          // 单张表大小（字节）
#define SYS_TABLE_HEAD_DATA1 0X68                 // 表头数据
#define SYS_TABLE_HEAD_DATA2 0X69                 // 表头数据
#define SYS_TABLE_TAIL_DATA1 0X86                 // 表尾数据
#define SYS_TABLE_TAIL_DATA2 0X87                 // 表尾数据
#define SYS_TABLE_HEAD1_INDEX 0                   // 表头索引
#define SYS_TABLE_HEAD2_INDEX 1                   // 表头索引
#define SYS_TABLE_NUMBER_INDEX 2                  // 表序号索引
#define SYS_TABLE_USER_DATA_INDEX 3               // 用户数据索引
#define SYS_TABLE_TAIL1_INDEX (STORAGE_SIZE - 2)  // 表尾索引
#define SYS_TABLE_TAIL2_INDEX (STORAGE_SIZE - 1)  // 表尾索引
#define SYS_TABLE_CRC_ADDRESS1 (STORAGE_SIZE - 4) // CRC 地址
#define SYS_TABLE_CRC_ADDRESS2 (STORAGE_SIZE - 3) // CRC 地址
#define DATA_SIZE (STORAGE_SIZE - 4)              // 有效数据 508 字节
#define HEADER 0x68
#define TAIL 0x86

// ========== 静态数据 ==========
// TODO:注意不能在两个任务中写
static volatile uint8_t s_current_version = 0;

/**
 * 构建表数据
 * @param buf: 表数据缓冲区
 * @param ver: 表版本号
 * @param data: 数据
 * @param size: 数据长度（字节）
 */
static void build_table(uint8_t *buf, uint8_t ver, const FlashStorage_t *data, uint16_t size) {
    memset(buf, 0xFF, STORAGE_SIZE);
    buf[SYS_TABLE_HEAD1_INDEX] = SYS_TABLE_HEAD_DATA1;
    buf[SYS_TABLE_HEAD2_INDEX] = SYS_TABLE_HEAD_DATA2;

    buf[SYS_TABLE_NUMBER_INDEX] = ver;
    memcpy(buf + SYS_TABLE_USER_DATA_INDEX, data, size);
    // CRC 校验范围：版本号 + 数据区（共 509 字节）
    uint16_t crc16 = CRC16_WithUniqueID_Software(buf, SYS_TABLE_CRC_ADDRESS1);
    buf[SYS_TABLE_CRC_ADDRESS1] = (uint8_t)(crc16 & 0xFF);
    buf[SYS_TABLE_CRC_ADDRESS2] = (uint8_t)(crc16 >> 8);
    buf[SYS_TABLE_TAIL1_INDEX] = SYS_TABLE_TAIL_DATA1;
    buf[SYS_TABLE_TAIL2_INDEX] = SYS_TABLE_TAIL_DATA2;
}

/**
 * @brief 检查表数据是否有效
 * @param buf: 表数据缓冲区
 */
static bool is_table_valid(const uint8_t *buf) {
    if (buf[SYS_TABLE_HEAD1_INDEX] != SYS_TABLE_HEAD_DATA1 || buf[SYS_TABLE_HEAD2_INDEX] != SYS_TABLE_HEAD_DATA2
        || buf[SYS_TABLE_TAIL1_INDEX] != SYS_TABLE_TAIL_DATA1 || buf[SYS_TABLE_TAIL2_INDEX] != SYS_TABLE_TAIL_DATA2) {
        return false;
    }
    uint16_t crc16 = CRC16_WithUniqueID_Software(buf, SYS_TABLE_CRC_ADDRESS1);
    if ((uint8_t)(crc16 & 0xFF) != buf[SYS_TABLE_CRC_ADDRESS1] || (uint8_t)(crc16 >> 8) != buf[SYS_TABLE_CRC_ADDRESS2]) {
        return false;
    }
    return true;
}

/**
 * @brief 从 Flash 读取表数据
 * @param addr: Flash 地址
 * @param buf: 表数据缓冲区
 */
static BSP_Flash_Status_t flash_program_table(uint32_t addr, const uint8_t *buf) {
    return BSP_FLASH_Write(addr, (const uint32_t *)buf, STORAGE_SIZE / sizeof(uint32_t));
}

/**
 * @brief 保存表数据到 Flash
 * @param buffer: 表数据缓冲区
 * @param number: 表序号（1 或 2）
 */
static BSP_Flash_Status_t save_table(uint8_t *buffer, uint8_t number) {
    uint32_t addr;
    if (number == 1) {
        addr = SYS_TABLE1_ADDR;
    } else {
        addr = SYS_TABLE2_ADDR;
    }

    // 擦除表所在页
    uint32_t page_addr = addr & ~(BSP_FLASH_PAGE_SIZE - 1); // 0x0807F000
    if (BSP_FLASH_ErasePage(page_addr) != BSP_FLASH_OK) {
        BSP_Flash_Protect_Exit();
        return BSP_FLASH_ERROR;
    }
    // 写入表
    if (flash_program_table(addr, buffer) != BSP_FLASH_OK) {
        BSP_Flash_Protect_Exit();
        return BSP_FLASH_ERROR; // 或者 SYS_PARTIAL_OK，但为了上层简单，返回 SYS_OK
    }

    return BSP_FLASH_OK;
}

// ========== 对外接口 ==========
/**
 * @brief 从 Flash 读取表数据
 * @param data: 表数据缓冲区
 * @param size: 数据长度（字节）
 * @return SYS_StatusTypeDef
 */
SYS_StatusTypeDef SYS_Storage_Load(FlashStorage_t *data, uint16_t size) {
    uint8_t table1_buf[STORAGE_SIZE];
    uint8_t table2_buf[STORAGE_SIZE];

    // 1. 从 Flash 读取两张表
    BSP_FLASH_Read(SYS_TABLE1_ADDR, (uint32_t *)table1_buf, STORAGE_SIZE / 4);
    BSP_FLASH_Read(SYS_TABLE2_ADDR, (uint32_t *)table2_buf, STORAGE_SIZE / 4);

    bool v1 = is_table_valid(table1_buf);
    bool v2 = is_table_valid(table2_buf);
    const uint8_t *selected = NULL;

    // 2. 决策逻辑
    if (!v1 && !v2) {
        s_current_version = 0;
        return SYS_ERROR;
    } else if (v1 && !v2) {
        selected = table1_buf;
    } else if (!v1 && v2) {
        selected = table2_buf;
    } else {
        // 两张表都有效，比较版本号
        uint8_t ver1 = table1_buf[SYS_TABLE_NUMBER_INDEX];
        uint8_t ver2 = table2_buf[SYS_TABLE_NUMBER_INDEX];

        if (ver1 == 0xFF && ver2 < 0x7F) {
            selected = table2_buf;
        } else if (ver2 == 0xFF && ver1 < 0X7F) {
            selected = table1_buf;
        } else {
            selected = (ver1 > ver2) ? table1_buf : table2_buf;
        }
    }

    // 3. 恢复数据
    if (selected) {
        memcpy(data, selected + SYS_TABLE_USER_DATA_INDEX, size);
        s_current_version = selected[SYS_TABLE_NUMBER_INDEX];
        return SYS_OK;
    } else {
        s_current_version = 0;
        return SYS_ERROR;
    }
}

/**
 * @brief 保存表数据到 Flash
 * @param data: 表数据缓冲区
 * @param size: 数据长度（字节）
 * @return SYS_StatusTypeDef
 */
SYS_StatusTypeDef SYS_Storage_Save(const FlashStorage_t *data, uint16_t size) {
    uint8_t new_ver = s_current_version + 1;
    uint8_t buffer[STORAGE_SIZE] __attribute__((aligned(4)));
    // 进入保护模式
    BSP_Flash_Protect_Enter();
    // 构建新表数据
    build_table(buffer, new_ver, data, size);
    if (new_ver % 2 == 0) {
        if (save_table(buffer, 1) != BSP_FLASH_OK) {
            return SYS_ERROR;
        }
    } else {
        if (save_table(buffer, 2) != BSP_FLASH_OK) {
            return SYS_ERROR;
        }
    }
    // 退出保护模式
    BSP_Flash_Protect_Exit();
    s_current_version = new_ver;
    return SYS_OK;
}