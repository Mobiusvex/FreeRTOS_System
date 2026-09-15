#ifndef BSP_IIC_H
#define BSP_IIC_H

#include "stm32f1xx_hal.h"
#include "sys_defs.h"
// 1. 定义总线实例（区分硬件和软件）
typedef enum {
    BSP_IIC_BUS_MPU6050_SW = 0, // 软件模拟SPI1 (接XPT2046)
    BSP_IIC_BUS_NUMBER
} BSP_IIC_Bus_t;

SYS_StatusTypeDef BSP_I2C_Write(BSP_IIC_Bus_t bus, uint8_t *pdata, uint16_t len, uint32_t timeout);
SYS_StatusTypeDef BSP_I2C_Read(BSP_IIC_Bus_t bus, uint16_t mem_addr, uint16_t mem_addr_size, uint8_t *pdata, uint16_t len, uint32_t timeout);
#endif