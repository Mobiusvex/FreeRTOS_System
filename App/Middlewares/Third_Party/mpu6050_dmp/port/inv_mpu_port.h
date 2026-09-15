#ifndef MPU6050_INV_MPU_PORT_H
#define MPU6050_INV_MPU_PORT_H
#include "stdint.h"

int i2c_write(unsigned char slave_addr, unsigned char reg_addr,
              unsigned char length, unsigned char const *data);
int i2c_read(unsigned char slave_addr, unsigned char reg_addr,
             unsigned char length, unsigned char *data);
void get_ms(unsigned long *tick);
void delay_ms(uint32_t ms);

#endif // MPU6050_INV_MPU_PORT_H
