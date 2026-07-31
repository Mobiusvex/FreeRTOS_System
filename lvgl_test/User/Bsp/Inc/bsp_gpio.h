// bsp_gpio.h
#ifndef __BSP_GPIO_H
#define __BSP_GPIO_H

#include <stdint.h>
#include <stdbool.h>

// 1. 定义逻辑电平状态（替代 HAL 的 GPIO_PinState，避免耦合）
typedef enum {
    BSP_GPIO_LOW = 0,
    BSP_GPIO_HIGH = 1
} BSP_GPIO_Level_t;

// 2. 定义板子上所有的逻辑 GPIO 功能（这是核心！）
typedef enum {
    // BSP_GPIO_SPEAKER_EN,    // 8002A 功放使能引脚 (高电平工作)
    // BSP_GPIO_LED_STATUS,    // 板载状态灯
    BSP_GPIO_XPT2046_SPI_CS, // XPT2046 SPI CS 引脚
    BSP_GPIO_XPT2046_SPI_CLK,
    BSP_GPIO_XPT2046_SPI_MOSI,
    BSP_GPIO_XPT2046_SPI_MISO,
    BSP_GPIO_XPT2046_PENIRQ,
    BSP_GPIO_LCD_BL, // 屏幕背光
    BSP_GPIO_LCD_RST,
    // 总数量（必须放在最后）
    BSP_GPIO_NUMBER
} BSP_GPIO_Name_t;

// 3. 对外提供的标准接口函数
void BSP_GPIO_Init(void); // 初始化所有引脚
void BSP_GPIO_Write(BSP_GPIO_Name_t name, BSP_GPIO_Level_t level);
BSP_GPIO_Level_t BSP_GPIO_Read(BSP_GPIO_Name_t name);
void BSP_GPIO_Toggle(BSP_GPIO_Name_t name);

#endif