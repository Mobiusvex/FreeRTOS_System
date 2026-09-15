#include "bsp_delay.h"
#include "cmsis_os2.h"

#include "stm32f1xx_hal.h" // 仅用于 HAL_GPIO_ReadPin/WritePin
#include "tim.h"
/**
 * @brief  简单微秒级延时函数
 * @param  nCount ：延时计数值，单位为微妙
 * @retval 无
 */
void BSP_DelayUS(uint32_t ulCount) {
    uint32_t lock_state = osKernelLock();

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
    osKernelRestoreLock(lock_state);
}

/**
 * @brief  忙等 毫秒级延时函数
 * @param  nCount ：延时计数值，单位为毫秒
 * @retval 无
 */
void BSP_DelayMS_Block(uint32_t ms) {
    HAL_Delay(ms);
}

/**
 * @brief  阻塞 毫秒级延时函数
 * @param  nCount ：延时计数值，单位为毫秒
 * @retval 无
 */
void BSP_DelayMS_Sleep(uint32_t ms) {
    if (osKernelGetState() == osKernelRunning) {
        osDelay(ms);
    } else {
        BSP_DelayMS_Block(ms); // 容错降级
    }
}

/**
 * @brief  获取当前系统时间
 * @retval 当前系统时间，单位为毫秒
 */
uint32_t BSP_GetTick(void) {
    return osKernelGetTickCount();
}