
#include "driver_w25q64.h"
#include "bootloader.h"
#include "main.h" /* HAL 定义 */
#include "driver_w25q64.h"
#include <string.h>
#include "SEGGER_RTT.h"

/* ============================================================
 *  CRC32（标准反射型，与 Python binascii.crc32 一致）
 * ============================================================ */
uint32_t BL_CRC32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFU;
    for (uint32_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (int k = 0; k < 8; k++)
        {
            crc = (crc & 1) ? (0xEDB88320U ^ (crc >> 1)) : (crc >> 1);
        }
    }
    return crc ^ 0xFFFFFFFFU;
}

/* ============================================================
 *  工具函数
 * ============================================================ */
static uint16_t get_u16_le(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t get_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* ============================================================
 *  检查外部 Flash 里是否有有效的新固件元信息
 * ============================================================ */
bool Bootloader_CheckMeta(OTA_Meta_t *meta)
{
    uint8_t buf[OTA_META_SIZE];

    // BSP_W25Qxx_BufferWrite(buf, OTA_META_ADDR, OTA_META_SIZE);
    /* 1. 读元信息 */
    if (!BSP_W25Qxx_BufferRead(buf, OTA_META_ADDR, OTA_META_SIZE))
    {
        //  SEGGER_RTT_printf(0, "BL: read meta failed\n");
        return false;
    }

    /* 2. 拷贝到结构体 */
    memcpy(meta, buf, sizeof(OTA_Meta_t));

    /* 3. 检查魔数 */
    if (meta->magic != OTA_MAGIC)
    {
        //  SEGGER_RTT_printf(0, "BL: no valid meta (magic=0x%08X)\n", meta->magic);
        return false;
    }

    /* 4. 校验元信息自身的CRC */
    uint32_t calc = BL_CRC32(buf, 12);
    if (calc != meta->meta_crc32)
    {
        //  SEGGER_RTT_printf(0, "BL: meta CRC err (calc=0x%08X recv=0x%08X)\n", calc, meta->meta_crc32);
        return false;
    }

    /* 5. 合理性检查 */
    if (meta->fw_size == 0 || meta->fw_size > 490 * 1024)
    {
        //  SEGGER_RTT_printf(0, "BL: fw_size out of range: %u\n", meta->fw_size);
        return false;
    }

    //  SEGGER_RTT_printf(0, "BL: meta OK, fw_size=%u, fw_crc=0x%08X\n", meta->fw_size, meta->fw_crc32);
    return true;
}

/* ============================================================
 *  校验外部 Flash 中的固件完整性
 *  - 逐页读、逐页校验页 CRC
 *  - 累积总 CRC，与元信息中的 fw_crc32 比对
 *  - 只读操作，不修改任何 Flash 内容
 *  - 校验失败时内部 Flash 完好，旧 APP 可继续使用
 * ============================================================ */
bool Bootloader_VerifyFirmware(const OTA_Meta_t *meta)
{
    uint16_t total_pages = (meta->fw_size + OTA_DATA_PER_PAGE - 1) / OTA_DATA_PER_PAGE;
    uint32_t crc = 0xFFFFFFFFU;
    uint8_t page[OTA_PAGE_SIZE];

    //  SEGGER_RTT_printf(0, "BL: verifying %u pages...\n", total_pages);

    for (uint16_t i = 0; i < total_pages; i++)
    {

        /* 1. 从 W25Q64 读一页 */
        uint32_t ext_addr = OTA_DATA_BASE_ADDR + (uint32_t)i * OTA_PAGE_SIZE;
        if (!BSP_W25Qxx_BufferRead(page, ext_addr, OTA_PAGE_SIZE))
        {
            //  SEGGER_RTT_printf(0, "BL: read page %u failed\n", i);
            return false;
        }

        /* 2. 校验本页 CRC */
        uint32_t pc_calc = BL_CRC32(page, OTA_DATA_PER_PAGE);
        uint32_t pc_stored = get_u32_le(&page[OTA_DATA_PER_PAGE]);
        if (pc_calc != pc_stored)
        {
            //  SEGGER_RTT_printf(0, "BL: page %u CRC err (calc=0x%08X stored=0x%08X)\n",i, pc_calc, pc_stored);
            return false;
        }

        /* 3. 累积总 CRC */
        for (uint16_t k = 0; k < OTA_DATA_PER_PAGE; k++)
        {
            crc ^= page[k];
            for (int b = 0; b < 8; b++)
            {
                crc = (crc & 1) ? (0xEDB88320U ^ (crc >> 1)) : (crc >> 1);
            }
        }

        /* 4. 定期喂狗 */
        if ((i & 0x3F) == 0x3F)
        {
            // HAL_IWDG_Refresh(&hiwdg);
        }
    }

    /* 5. 总 CRC 比对 */
    crc ^= 0xFFFFFFFFU;
    if (crc != meta->fw_crc32)
    {
        //  SEGGER_RTT_printf(0, "BL: total CRC mismatch! calc=0x%08X expect=0x%08X\n",crc, meta->fw_crc32);
        return false;
    }

    //  SEGGER_RTT_printf(0, "BL: verify OK\n");
    return true;
}

/* ============================================================
 *  从 W25Q64 逐页读出，校验页CRC + 累积总CRC，写入内部Flash
 * ============================================================ */
bool Bootloader_LoadFirmware(const OTA_Meta_t *meta)
{
    /* 1. 计算需要的页数（向上取整） */
    uint16_t total_pages = (meta->fw_size + OTA_DATA_PER_PAGE - 1) / OTA_DATA_PER_PAGE;
    uint32_t padded_size = (uint32_t)total_pages * OTA_DATA_PER_PAGE;

    //  SEGGER_RTT_printf(0, "BL: loading %u pages (%u bytes padded)\n", total_pages, padded_size);

    /* 2. 擦除内部 Flash 的 APP 区 */
    //  SEGGER_RTT_printf(0, "BL: erasing internal flash...\n");
    if (!InternalFlash_Erase(INTERNAL_APP_ADDR, padded_size))
    {
        //  SEGGER_RTT_printf(0, "BL: erase internal flash failed\n");
        return false;
    }

    /* 3. 逐页处理 */
    uint32_t crc = 0xFFFFFFFFU;
    uint8_t page[OTA_PAGE_SIZE];

    for (uint16_t i = 0; i < total_pages; i++)
    {

        /* 3.1 从 W25Q64 读一页 */
        uint32_t ext_addr = OTA_DATA_BASE_ADDR + (uint32_t)i * OTA_PAGE_SIZE;
        if (!BSP_W25Qxx_BufferRead(page, ext_addr, OTA_PAGE_SIZE))
        {
            //  SEGGER_RTT_printf(0, "BL: read page %u failed\n", i);
            return false;
        }

        /* 3.2 校验本页 CRC */
        uint32_t pc_calc = BL_CRC32(page, OTA_DATA_PER_PAGE);
        uint32_t pc_stored = get_u32_le(&page[OTA_DATA_PER_PAGE]);
        if (pc_calc != pc_stored)
        {
            //  SEGGER_RTT_printf(0, "BL: page %u CRC err (calc=0x%08X stored=0x%08X)\n", i, pc_calc, pc_stored);
            return false;
        }

        /* 3.3 累积总 CRC */
        for (uint16_t k = 0; k < OTA_DATA_PER_PAGE; k++)
        {
            crc ^= page[k];
            for (int b = 0; b < 8; b++)
            {
                crc = (crc & 1) ? (0xEDB88320U ^ (crc >> 1)) : (crc >> 1);
            }
        }

        /* 3.4 写入内部 Flash */
        uint32_t dst = INTERNAL_APP_ADDR + (uint32_t)i * OTA_DATA_PER_PAGE;
        if (!InternalFlash_Write(dst, page, OTA_DATA_PER_PAGE))
        {
            //  SEGGER_RTT_printf(0, "BL: write internal flash @0x%08X failed\n", dst);
            return false;
        }

        /* 3.5 喂狗 + 心跳 */
        if ((i & 0x0F) == 0x0F)
        {
            // TODO:喂狗
            //  extern IWDG_HandleTypeDef hiwdg;
            //  HAL_IWDG_Refresh(&hiwdg);
            //  RTT_PRINTF("BL: %u/%u pages loaded\n", i + 1, total_pages);
        }
    }

    /* 4. 总 CRC 比对 */
    crc ^= 0xFFFFFFFFU;
    if (crc != meta->fw_crc32)
    {
        //  SEGGER_RTT_printf(0, "BL: total CRC mismatch! calc=0x%08X expect=0x%08X\n", crc, meta->fw_crc32);
        return false;
    }

    //  SEGGER_RTT_printf(0, "BL: firmware loaded, total CRC OK\n");
    return true;
}

/* ============================================================
 *  清除元信息（擦除魔数，避免重复加载）
 * ============================================================ */
void Bootloader_InvalidateMeta(void)
{
    BSP_W25Qxx_SectorErase(OTA_META_ADDR);
    //  SEGGER_RTT_printf(0, "BL: meta deleted\n");
}

/* ============================================================
 *  检查内部 Flash 的 APP 是否有效
 * ============================================================ */
bool Bootloader_IsAppValid(uint32_t app_addr)
{
    uint32_t sp = *(__IO uint32_t *)app_addr;
    uint32_t pc = *(__IO uint32_t *)(app_addr + 4);

    /* SP 应在 SRAM 范围内 */
    if (sp < 0x20000000U || sp > 0x20020000U)
    {
        //  SEGGER_RTT_printf(0, "BL: APP SP invalid: 0x%08X\n", sp);
        return false;
    }

    /* PC 应在 Flash 范围内，且是奇数（Thumb） */
    if (pc < 0x08000000U || pc > 0x08100000U || (pc & 1) == 0)
    {
        //  SEGGER_RTT_printf(0, "BL: APP PC invalid: 0x%08X\n", pc);
        return false;
    }

    return true;
}

/* ============================================================
 *  跳转到 APP
 * ============================================================ */
typedef void (*pFunction)(void);

void Bootloader_JumpToApp(uint32_t app_addr)
{
    uint32_t app_sp = *(__IO uint32_t *)app_addr;
    uint32_t app_pc = *(__IO uint32_t *)(app_addr + 4);
    pFunction jump = (pFunction)app_pc;

    //  SEGGER_RTT_printf(0, "BL: jumping to APP @0x%08X (SP=0x%08X)\n", app_addr, app_sp);

    /* 1. 关闭全局中断 */
    __disable_irq();

    /* 2. 关闭 SysTick、复位外设 */
    HAL_DeInit();

    /* 3. 关闭所有 NVIC 中断 */
    for (uint8_t i = 0; i < 8; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    /* 4. 设置向量表 */
    SCB->VTOR = app_addr;
    __DSB();
    __ISB();
    // /* 5. 设置 MSP */
    // __set_MSP(app_sp);

    // /* 6. 使能全局中断（由 APP 自己控制） */
    // __enable_irq();

    /* 7. 跳转 */
    jump();

    /* 不会到这里 */
    while (1)
    {
    }
}

/* ============================================================
 *  内部 Flash 擦除（按扇区/页）
 *  ★ 需要根据具体 STM32 型号调整
 * ============================================================ */
bool InternalFlash_Erase(uint32_t addr, uint32_t size)
{
    HAL_FLASH_Unlock();

    uint32_t pages = (size + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE;

    FLASH_EraseInitTypeDef erase;
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = addr;
    erase.NbPages = 1; /* ★ 每次只擦一页 */

    for (uint32_t i = 0; i < pages; i++)
    {
        uint32_t page_error = 0;
        if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return false;
        }
        erase.PageAddress += FLASH_PAGE_SIZE;

        /* 每页擦除后喂狗 + 让出CPU */
        // HAL_IWDG_Refresh(&hiwdg);
    }

    HAL_FLASH_Lock();
    return true;
}

/* ============================================================
 *  内部 Flash 写入
 *  ★ 需要根据具体 STM32 型号调整
 * ============================================================ */
bool InternalFlash_Write(uint32_t addr, const uint8_t *data, uint32_t len)
{
    HAL_FLASH_Unlock();

    /* ---- F1 系列：按半字（16位）编程 ---- */
    for (uint32_t i = 0; i < len; i += 2)
    {
        uint16_t halfword = (uint16_t)data[i] | ((uint16_t)data[i + 1] << 8);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + i, halfword) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return false;
        }
    }

    /* ---- F4/F7 系列：按字（32位）编程 ----
    for (uint32_t i = 0; i < len; i += 4) {
        uint32_t word = (uint32_t)data[i]
                      | ((uint32_t)data[i+1] << 8)
                      | ((uint32_t)data[i+2] << 16)
                      | ((uint32_t)data[i+3] << 24);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i, word) != HAL_OK) {
            HAL_FLASH_Lock();
            return false;
        }
    }
    */

    HAL_FLASH_Lock();
    return true;
}

/* ============================================================
 *  Bootloader 主入口
 * ============================================================ */
void Bootloader_Run(void)
{
    //  SEGGER_RTT_printf(0, "\n===== STM32 Bootloader =====\n");
    OTA_Meta_t meta;

    /* 1. 检查外部 Flash 是否有待加载的固件 */
    if (Bootloader_CheckMeta(&meta) && Bootloader_VerifyFirmware(&meta))
    {

        //  SEGGER_RTT_printf(0, "BL: pending firmware found, loading...\n");
        /* 2. 加载固件到内部 Flash */
        if (Bootloader_LoadFirmware(&meta))
        {
            //  SEGGER_RTT_printf(0, "BL: firmware loaded OK\n");

            /* 3. 清除元信息，避免下次重复加载 */
            Bootloader_InvalidateMeta();

            /* 4. 跳转前检查 APP 有效性 */
            if (!Bootloader_IsAppValid(INTERNAL_APP_ADDR))
            {
                //  SEGGER_RTT_printf(0, "BL: APP invalid after load!\n");
                /* 进升级模式或死循环 */
                while (1)
                {
                }
            }

            /* 5. 跳转 */
            Bootloader_JumpToApp(INTERNAL_APP_ADDR);
        }
        else
        {
            //  SEGGER_RTT_printf(0, "BL: firmware load FAILED\n");

            /* 加载失败：清掉元信息，避免下次又尝试加载坏的固件 */
            Bootloader_InvalidateMeta();

            /* 尝试跳转到旧的 APP */
            if (Bootloader_IsAppValid(INTERNAL_APP_ADDR))
            {
                //  SEGGER_RTT_printf(0, "BL: fallback to old APP\n");
                Bootloader_JumpToApp(INTERNAL_APP_ADDR);
            }

            /* 旧 APP 也不可用：停在 Bootloader */
            //  SEGGER_RTT_printf(0, "BL: no valid APP, stay in bootloader\n");
        }
    }
    else
    {
        //  SEGGER_RTT_printf(0, "BL: no pending firmware\n");
        /* 没有待加载的固件，直接跳转到 APP */
        if (Bootloader_IsAppValid(INTERNAL_APP_ADDR))
        {
            Bootloader_JumpToApp(INTERNAL_APP_ADDR);
        }

        //  SEGGER_RTT_printf(0, "BL: no valid APP, stay in bootloader\n");
    }

    /* 走到这里说明都失败了，进入升级等待模式（可选实现） */
    while (1)
    {
        /* 可以在这里实现：等待 UART 升级、按键进入升级等 */
        HAL_Delay(1000);
        //  SEGGER_RTT_printf(0, "BL: waiting for upgrade...\n");
    }
}