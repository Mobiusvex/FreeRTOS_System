// inv_mpu_port.c
#include "inv_mpu.h"   // 包含 DMP 库头文件，让编译器知道需要实现哪些函数
#include "bsp_iic.h"   // ★ 包含你的 BSP I2C 抽象层
#include "bsp_delay.h" // ★ 包含你的 BSP 延时抽象层

// ========== 1. 定义 MPU6050 挂载在哪条 I2C 总线上 ==========
#define MPU6050_I2C_BUS BSP_IIC_BUS_1 // 假设你定义了一个 BSP_IIC_BUS_1

// ========== 2. 实现 DMP 库需要的 i2c_write ==========
// 注意：inv_mpu.c 里定义的函数原型通常是：
// int i2c_write(unsigned char slave_addr, unsigned char reg_addr,
//               unsigned char length, unsigned char const *data)
int i2c_write(unsigned char slave_addr, unsigned char reg_addr,
              unsigned char length, unsigned char const *data) {
    // 手动构造发送缓冲区 [reg_addr] + [data...]
    uint8_t tx_buf[25];
    if ((1 + length) > sizeof(tx_buf)) return -1; // 防溢出

    tx_buf[0] = reg_addr;
    for (int i = 0; i < length; i++) {
        tx_buf[1 + i] = data[i];
    }

    // 调用你的 BSP 接口（注意：地址需要左移1位，BSP层如果没处理则需要在这里处理）
    // 假设你的 BSP 层内部处理了 7bit 地址移位，或者外部需要移位，视你的 BSP 设计而定
    SYS_StatusTypeDef ret = BSP_I2C_Write(BSP_IIC_BUS_MPU6050_SW, tx_buf, 1 + length, 100);

    return (ret == SYS_OK) ? 0 : -1; // DMP 库通常约定 0 为成功，-1 为失败
}

int i2c_read(unsigned char slave_addr, unsigned char reg_addr,
             unsigned char length, unsigned char *data) {
    SYS_StatusTypeDef ret = BSP_I2C_Read(BSP_IIC_BUS_MPU6050_SW,
                                         (uint16_t)reg_addr, // mem_addr
                                         1,                  // mem_addr_size (8bit)
                                         data,               // pdata
                                         length,             // len
                                         100);               // timeout_ms

    return (ret == SYS_OK) ? 0 : -1;
}

void get_ms(unsigned long *tick) {
    if (tick != NULL) {
        *tick = (unsigned long)HAL_GetTick(); // 解引用赋值
    }
}

void delay_ms(uint32_t ms) {
    if (ms >= 5) {
        BSP_DelayMS_Sleep(ms);
    } else {
        BSP_DelayMS_Block(ms);
    }
}