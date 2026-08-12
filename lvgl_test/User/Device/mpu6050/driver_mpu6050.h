#ifndef __DRIVER_MPU6050_H__
#define __DRIVER_MPU6050_H__

#include "stdint.h"
#include "sys_defs.h"
#define q30 1073741824.0f

extern float Pitch, Roll, Yaw;
extern short gyro[3], accel[3];
// extern float gyro[0], gyro[1], gyro[2], accel[0],accel[1],accel[2];

SYS_StatusTypeDef MPU6050_Init(void);
void MPU6050_Pose(void);
SYS_StatusTypeDef MPU6050_getAngle(float *pitch, float *roll, float *yaw);

#endif