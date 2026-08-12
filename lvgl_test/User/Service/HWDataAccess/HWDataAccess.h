#ifndef HWDATAACCESS_H
#define HWDATAACCESS_H
#include "cmsis_os2.h"
#include "sys_defs.h"

typedef struct {
    uint8_t ConnectionError;
    uint16_t update_time;
    SYS_StatusTypeDef data_status;
    uint8_t temperature;
    uint8_t humidity;
    SYS_StatusTypeDef (*Init)(void);
    SYS_StatusTypeDef (*GetHumiTemp)(float *humi, float *temp);
} HW_DHT11_InterfaceTypeDef;

typedef struct
{
    uint8_t ConnectionError;
    uint16_t update_time;
    SYS_StatusTypeDef data_status;
    float pitch_angle;
    float roll_angle;
    float yaw_angle;

    SYS_StatusTypeDef (*Init)(void);
    SYS_StatusTypeDef (*GetAngle)(float *pitch, float *roll, float *yaw);
} HW_MPU6050_InterfaceTypeDef;

typedef struct {
    HW_DHT11_InterfaceTypeDef DHT11;
    HW_MPU6050_InterfaceTypeDef MPU6050;
} HW_InterfaceTypeDef;

extern HW_InterfaceTypeDef HW_Interface;

#endif
