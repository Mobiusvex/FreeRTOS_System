#ifndef __BSP_FLASH_H
#define __BSP_FLASH_H

#include <stdint.h>
#include <stddef.h>

// 板级配置宏
#define BSP_FLASH_PAGE_SIZE (2048U)         // 2KB per page
#define BSP_FLASH_TOTAL_SIZE (512U * 1024U) // 512KB (根据实际芯片修改)
#define BSP_FLASH_BASE_ADDR (0x08000000UL)
#define BSP_FLASH_LAST_PAGE_START (BSP_FLASH_BASE_ADDR + BSP_FLASH_TOTAL_SIZE - BSP_FLASH_PAGE_SIZE)

// 返回状态（扩展HAL状态，增加自定义错误）
typedef enum {
    BSP_FLASH_OK = 0,
    BSP_FLASH_ERROR,
    BSP_FLASH_BUSY,
    BSP_FLASH_TIMEOUT,
    BSP_FLASH_ADDR_INVALID,
    BSP_FLASH_SIZE_INVALID,
    BSP_FLASH_NOT_ALIGNED,
} BSP_Flash_Status_t;

// 公共接口
BSP_Flash_Status_t BSP_FLASH_ErasePage(uint32_t PageStartAddress);
BSP_Flash_Status_t BSP_FLASH_Write(uint32_t Address, const uint32_t *pData, size_t WordCount);
BSP_Flash_Status_t BSP_FLASH_Read(uint32_t Address, uint32_t *pBuffer, size_t WordCount);
uint32_t BSP_FLASH_GetPageSize(void);
uint32_t BSP_FLASH_GetTotalSize(void);

#endif