#include "bsp_system.h"
#include "stm32f1xx_hal.h"

void BSP_SystemReset(void) {
    HAL_NVIC_SystemReset();
}