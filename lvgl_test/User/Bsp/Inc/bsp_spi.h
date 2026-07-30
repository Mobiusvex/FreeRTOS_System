// bsp_spi.h
#ifndef __BSP_SPI_H
#define __BSP_SPI_H

#include <stdint.h>
#include <stdbool.h>

// 1. 定义总线实例（区分硬件和软件）
typedef enum {
    BSP_SPI_BUS_XPT2046 = 0, // 软件模拟SPI1 (接XPT2046)
    // BSP_SPI_BUS_1,  // 硬件SPI1 (接W25Q64)
    // BSP_SPI_BUS_2,      // 硬件SPI2 (预留)
    BSP_SPI_BUS_NUMBER
} BSP_SPI_Bus_t;

// 2. 配置结构体（调用者指定速率和模式）
typedef struct {
    bool is_software;  // true: 软件模拟, false: 硬件外设
    uint32_t baudrate; // 硬件SPI时钟（如 18000000），软件SPI则对应延迟时间(us)
    uint8_t cpol;      // 时钟极性 (0/1)
    uint8_t cpha;      // 时钟相位 (0/1)
} BSP_SPI_Config_t;

// 3. 唯一对外接口（上层永远只调这几个）
bool BSP_SPI_Init(BSP_SPI_Bus_t bus, const BSP_SPI_Config_t *cfg);
bool BSP_SPI_TransmitReceive(BSP_SPI_Bus_t bus,
                             const uint8_t *tx_data,
                             uint8_t *rx_data,
                             uint16_t length,
                             uint32_t timeout_ms);

void BSP_DelayUS(volatile uint32_t ulCount);
#endif