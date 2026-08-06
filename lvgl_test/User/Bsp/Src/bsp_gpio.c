// bsp_gpio.c
#include "bsp_gpio.h"
#include "stm32f1xx_hal.h" // 仅用于 HAL_GPIO_ReadPin/WritePin
#include "FreeRTOS.h"
#include "task.h"

// 1. 精简映射表：只保留 端口 和 引脚 (去掉 mode, pull, init_level)
typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} GPIO_Map_t;

// 2. 映射表定义 (★ 只负责把逻辑功能对应到物理引脚 ★)
static const GPIO_Map_t s_gpioMap[BSP_GPIO_NUMBER] = {
    [BSP_GPIO_XPT2046_SPI_CS] = {GPIOF, GPIO_PIN_10},
    [BSP_GPIO_XPT2046_SPI_CLK] = {GPIOG, GPIO_PIN_7},
    [BSP_GPIO_XPT2046_SPI_MOSI] = {GPIOF, GPIO_PIN_11},
    [BSP_GPIO_XPT2046_SPI_MISO] = {GPIOF, GPIO_PIN_6},
    [BSP_GPIO_XPT2046_PENIRQ] = {GPIOF, GPIO_PIN_9},
    [BSP_GPIO_LCD_BL] = {GPIOG, GPIO_PIN_6},
    [BSP_GPIO_LCD_RST] = {GPIOG, GPIO_PIN_11},
    [BSP_GPIO_DHT11_DATA] = {GPIOD, GPIO_PIN_6},
};

/**
 * @brief 初始化 GPIO 引脚
 * @param 无
 * @retval 无
 * @details 初始化 GPIO 引脚，设置输出引脚的默认安全电平
 */
void BSP_GPIO_Init(void) {
    // ★★★ 核心变更：这里只设置输出引脚的默认安全电平 ★★★
    // 假设你希望上电默认：关背光、静音、灭状态灯
    // BSP_GPIO_Write(BSP_GPIO_LCD_BL, BSP_GPIO_LOW);

    // 如果触摸中断引脚配置了外部中断，CubeMX 已经初始化好了，这里无需任何操作。
}
/**
 * @brief 设置 GPIO 引脚电平
 * @param name GPIO 引脚名称
 * @param level 电平
 * @retval 无
 */
void BSP_GPIO_Write(BSP_GPIO_Name_t name, BSP_GPIO_Level_t level) {
    if (name >= BSP_GPIO_NUMBER) return;
    HAL_GPIO_WritePin(s_gpioMap[name].port, s_gpioMap[name].pin, (GPIO_PinState)level);
}

/**
 * @brief 读取 GPIO 引脚电平
 * @param name GPIO 引脚名称
 * @retval 电平
 */
BSP_GPIO_Level_t BSP_GPIO_Read(BSP_GPIO_Name_t name) {
    if (name >= BSP_GPIO_NUMBER) return BSP_GPIO_LOW;
    return (BSP_GPIO_Level_t)HAL_GPIO_ReadPin(s_gpioMap[name].port, s_gpioMap[name].pin);
}

/**
 * @brief 翻转 GPIO 引脚电平
 * @param name GPIO 引脚名称
 * @retval 无
 */
void BSP_GPIO_Toggle(BSP_GPIO_Name_t name) {
    if (name >= BSP_GPIO_NUMBER) return;
    HAL_GPIO_TogglePin(s_gpioMap[name].port, s_gpioMap[name].pin);
}

/**
 * @brief 进入临界区
 * @param 无
 * @retval 无
 * @details 进入临界区，防止中断干扰
 */
void BSP_CRITICAL_Enter(void) {
    // TODO:记得改configMAX_SYSCALL_INTERRUPT_PRIORITY，让UART等中断优先级高于此
    taskENTER_CRITICAL();
}

/**
 * @brief 退出临界区
 */
void BSP_CRITICAL_Exit(void) {
    taskEXIT_CRITICAL();
}