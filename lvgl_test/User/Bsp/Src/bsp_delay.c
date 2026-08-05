#include "bsp_delay.h"
#include "stm32f1xx_hal.h" // 仅用于 HAL_GPIO_ReadPin/WritePin
#include "tim.h"
/**
 * @brief  简单微秒级延时函数
 * @param  nCount ：延时计数值，单位为微妙
 * @retval 无
 */
void BSP_DelayUS(uint32_t ulCount) {
    HAL_TIM_Base_Start(&htim7); // 可以只开启一次

    while (ulCount > 0) {
        // 每次最多延时 65535us（硬件CNT最大值）
        uint32_t wait = (ulCount > 0xFFFF) ? 0xFFFF : ulCount;

        __HAL_TIM_SET_COUNTER(&htim7, 0); // 每次清零从头计
        uint32_t start = __HAL_TIM_GET_COUNTER(&htim7);

        // 等待差值达到 wait
        while ((__HAL_TIM_GET_COUNTER(&htim7) - start) < wait) {
            // 空循环
        }
        ulCount -= wait; // 剩余时间继续
    }

    HAL_TIM_Base_Stop(&htim7);
}

/**
 * @brief  简单毫秒级延时函数
 * @param  nCount ：延时计数值，单位为毫妙
 * @retval 无
 */
void BSP_DelayMS(uint32_t mlCount) {
    for (uint16_t i = 0; i < mlCount; i++) {
        BSP_DelayUS(1000);
    }
}