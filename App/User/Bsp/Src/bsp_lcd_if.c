#include "bsp_lcd_if.h"
#include "stm32f1xx_hal.h"
#include "bsp_fsmc.h"

/**
 * @brief 写命令（uint16_t）到LCD控制器
 * @param cmd 要发送的命令
 * @retval 无
 */
void BSP_LCD_IF_WriteCmd(uint16_t cmd) {
    *(volatile uint16_t *)(FSMC_ADDR_ILI9341_CMD) = cmd;
}

/**
 * @brief 写数据（uint16_t）到LCD控制器
 * @param data 要发送的数据
 * @retval 无
 */
void BSP_LCD_IF_WriteData(uint16_t data) {
    *(volatile uint16_t *)(FSMC_ADDR_ILI9341_DATA) = data;
}

/**
 * @brief 读数据（uint16_t）从LCD控制器
 * @param 无
 * @retval 无
 */
uint16_t BSP_LCD_IF_ReadData(void) {
    return (*(volatile uint16_t *)(FSMC_ADDR_ILI9341_DATA));
}