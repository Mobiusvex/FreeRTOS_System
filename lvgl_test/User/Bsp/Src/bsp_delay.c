#include "bsp_delay.h"
#include "stm32f1xx_hal.h" // 仅用于 HAL_GPIO_ReadPin/WritePin

/**
 * @brief  简单微秒级延时函数
 * @param  nCount ：延时计数值，单位为微妙
 * @retval 无
 */
void BSP_DelayUS(uint32_t ulCount) {
    uint32_t i;
    for (i = 0; i < ulCount; i++) {
        volatile uint8_t uc = 12; // 设置值为12，大约延1微秒

        while (uc--); // 延1微秒
    }
}

/**
 * @brief  简单毫秒级延时函数
 * @param  nCount ：延时计数值，单位为毫妙
 * @retval 无
 */
void BSP_DelayMS(uint32_t mlCount) {
    HAL_Delay(mlCount);
}