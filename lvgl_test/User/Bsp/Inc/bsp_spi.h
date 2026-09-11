// bsp_spi.h
#ifndef __BSP_SPI_H
#define __BSP_SPI_H

#include <stdint.h>
#include <stdbool.h>
#include "bsp_gpio.h"

// 1. 定义总线实例（区分硬件和软件）
typedef enum {
    BSP_SPI_BUS_XPT2046 = 0, // 软件模拟SPI1 (接XPT2046)
    BSP_SPI_BUS_1,           // 硬件SPI1 (接W25Q64)
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

typedef enum {
    BSP_SPI_OP_TX_ONLY = 0, /* 只发送        */
    BSP_SPI_OP_RX_ONLY,     /* 只接收        */
    BSP_SPI_OP_TX_RX,       /* 同时收发(等长) */
    BSP_SPI_OP_TX_THEN_RX,  /* 先发后收(不同长) */
} BSP_SPI_Op_t;

// 3. 唯一对外接口（上层永远只调这几个）
bool BSP_SPI_Init(BSP_SPI_Bus_t bus, const BSP_SPI_Config_t *cfg);

bool BSP_SPI_Write(BSP_SPI_Bus_t bus, BSP_GPIO_Name_t cs,
                   const uint8_t *tx, uint16_t len, uint32_t timeout_ms);

bool BSP_SPI_Read(BSP_SPI_Bus_t bus, BSP_GPIO_Name_t cs,
                  uint8_t *rx, uint16_t len, uint32_t timeout_ms);

bool BSP_SPI_WriteRead(BSP_SPI_Bus_t bus, BSP_GPIO_Name_t cs,
                       const uint8_t *tx, uint8_t *rx,
                       uint16_t len, uint32_t timeout_ms);

bool BSP_SPI_WriteThenRead(BSP_SPI_Bus_t bus, BSP_GPIO_Name_t cs,
                           const uint8_t *tx, uint16_t tx_len,
                           uint8_t *rx, uint16_t rx_len,
                           uint32_t timeout_ms);

#endif