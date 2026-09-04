#include "bsp_flash.h"
#include "stm32f1xx_hal.h"

/**
 * @brief HAL状态转换为BSP状态
 * @param hal_status HAL状态
 * @retval BSP状态
 */
static BSP_Flash_Status_t HAL_Status_to_BSP(HAL_StatusTypeDef hal_status) {
    switch (hal_status) {
    case HAL_OK: return BSP_FLASH_OK;
    case HAL_BUSY: return BSP_FLASH_BUSY;
    case HAL_TIMEOUT: return BSP_FLASH_TIMEOUT;
    default: return BSP_FLASH_ERROR;
    }
}

/**
 * @brief 擦除指定页
 * @param PageStartAddress 要擦除的页起始地址
 * #retval 擦除成功或失败
 */
BSP_Flash_Status_t BSP_FLASH_ErasePage(uint32_t PageStartAddress) {
    // 参数校验：地址是否在Flash范围内且页对齐
    if (PageStartAddress < BSP_FLASH_BASE_ADDR || PageStartAddress >= (BSP_FLASH_BASE_ADDR + BSP_FLASH_TOTAL_SIZE) || (PageStartAddress % BSP_FLASH_PAGE_SIZE) != 0) {
        return BSP_FLASH_ADDR_INVALID;
    }

    HAL_StatusTypeDef hal_status;
    FLASH_EraseInitTypeDef erase_init = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .PageAddress = PageStartAddress,
        .NbPages = 1,
    };
    uint32_t page_error = 0;

    HAL_FLASH_Unlock();
    hal_status = HAL_FLASHEx_Erase(&erase_init, &page_error);
    HAL_FLASH_Lock();

    // 如果擦除失败且 page_error 非0，可以记录错误页，但这里简单返回
    if (hal_status != HAL_OK) {
        return HAL_Status_to_BSP(hal_status);
    }
    if (page_error != 0xFFFFFFFF) { // 发生错误时的默认值
        return BSP_FLASH_ERROR;
    }
    return BSP_FLASH_OK;
}

/** @brief 写入数据，按字（32位）写入数据，自动处理地址对齐（强制4字节对齐）
 * @param Address 要写入数据的起始地址
 * @param pData 要写入的数据
 * @param WordCount 要写入的数据字数
 * @retval 写入成功或失败
 */
BSP_Flash_Status_t BSP_FLASH_Write(uint32_t Address, const uint32_t *pData, size_t WordCount) {
    // 参数校验
    if (pData == NULL || WordCount == 0) {
        return BSP_FLASH_SIZE_INVALID;
    }
    if (Address < BSP_FLASH_BASE_ADDR || Address + WordCount * 4 > BSP_FLASH_BASE_ADDR + BSP_FLASH_TOTAL_SIZE) {
        return BSP_FLASH_ADDR_INVALID;
    }
    if (Address % 4 != 0) {
        return BSP_FLASH_NOT_ALIGNED; // STM32写字必须4字节对齐
    }

    HAL_StatusTypeDef hal_status = HAL_OK;
    HAL_FLASH_Unlock();

    for (size_t i = 0; i < WordCount; i++) {
        hal_status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, Address + i * 4, pData[i]);
        if (hal_status != HAL_OK) {
            break;
        }
    }

    HAL_FLASH_Lock();
    return HAL_Status_to_BSP(hal_status);
}

/** @brief 从Flash中读取数据
 * @param Address 要读取数据的起始地址
 * @param pBuffer 存储读取数据的缓冲区
 * @param WordCount 要读取的数据字数
 * @retval 读取成功或失败
 */
BSP_Flash_Status_t BSP_FLASH_Read(uint32_t Address, uint32_t *pBuffer, size_t WordCount) {
    if (pBuffer == NULL || WordCount == 0) {
        return BSP_FLASH_SIZE_INVALID;
    }
    if (Address < BSP_FLASH_BASE_ADDR || Address + WordCount * 4 > BSP_FLASH_BASE_ADDR + BSP_FLASH_TOTAL_SIZE) {
        return BSP_FLASH_ADDR_INVALID;
    }
    // 读取不要求对齐，但通常建议字对齐提高效率
    for (size_t i = 0; i < WordCount; i++) {
        pBuffer[i] = *(__IO uint32_t *)(Address + i * 4);
    }
    return BSP_FLASH_OK;
}

/** @brief 获取Flash页大小
 * @retval Flash页大小
 */
uint32_t BSP_FLASH_GetPageSize(void) {
    return BSP_FLASH_PAGE_SIZE;
}

/** @brief 获取Flash总大小
 * @retval Flash总大小
 */
uint32_t BSP_FLASH_GetTotalSize(void) {
    return BSP_FLASH_TOTAL_SIZE;
}

void BSP_Flash_Protect_Enter(void) {
    __disable_irq(); // 关全局中断（防止擦写被打断）
}

void BSP_Flash_Protect_Exit(void) {
    __enable_irq();
}
