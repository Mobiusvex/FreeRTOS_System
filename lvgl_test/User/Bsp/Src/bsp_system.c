#include "bsp_system.h"
#include "stm32f1xx_hal.h"

void BSP_SystemReset(void) {
    HAL_NVIC_SystemReset();
}

void OTA_ResetToBootloader(void) {
    /* 1. 关闭所有外设中断（防止复位前的窗口期触发异常）*/
    __disable_irq();

    /* 2. 关掉 SysTick（FreeRTOS 停止调度）*/
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    /* 3. 清所有 NVIC 中断（防止挂起的中断在复位前触发）*/
    for (int i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    /* 4. 内存屏障，保证前面的操作全部生效 */
    __DSB();
    __ISB();

    /* 5. 触发系统复位 */
    HAL_NVIC_SystemReset();

    /* 不会到这里 */
    while (1) {}
}