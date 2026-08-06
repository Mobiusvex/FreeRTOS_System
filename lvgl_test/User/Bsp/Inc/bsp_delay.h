#ifndef BSP_DELAY_H
#define BSP_DELAY_H

#include <stdint.h>
#include <stdbool.h>

void BSP_DelayUS(uint32_t ulCount);

void BSP_DelayMS_Block(uint32_t ms); // 忙等
void BSP_DelayMS_Sleep(uint32_t ms); // 阻塞

#endif
