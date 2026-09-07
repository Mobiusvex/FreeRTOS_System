#ifndef HWDATAACCESS_H
#define HWDATAACCESS_H
#include "cmsis_os2.h"
#include "sys_defs.h"
#include "time_convert.h"
typedef struct {
    uint8_t ConnectionError;
    uint16_t update_time;
    SYS_StatusTypeDef data_status;
    SYS_StatusTypeDef (*Init)(void);
    SYS_StatusTypeDef (*GetHumiTemp)(int16_t *humi, int16_t *temp);
} HW_DHT11_InterfaceTypeDef;

typedef struct
{
    uint8_t ConnectionError;
    uint16_t update_time;
    SYS_StatusTypeDef data_status;
    SYS_StatusTypeDef (*Init)(void);
    SYS_StatusTypeDef (*GetAngle)(float *pitch, float *roll, float *yaw);
    void (*AngleFloatToInt)(float pitch, float roll, float yaw, int16_t *pitch_int, int16_t *roll_int, int16_t *yaw_int);
} HW_MPU6050_InterfaceTypeDef;

typedef struct
{
    uint16_t head;
    uint16_t tail;
} HW_Uart_ReceiverTypeDef;

typedef struct
{
    uint8_t ConnectionError;
    uint32_t time_fetch_interval;
    uint32_t onenet_report_interval;
    SYS_StatusTypeDef data_status;
    SYS_StatusTypeDef (*Init)(void);
    SYS_StatusTypeDef (*Reset)(void);
} HW_ESP8266_InterfaceTypeDef;

typedef struct
{
    SYS_StatusTypeDef data_status;
    uint16_t update_time;
    SYS_StatusTypeDef (*GetTimeString)(char *, uint8_t);
    SYS_StatusTypeDef (*SetTimedata)(datetime_t *);
    SYS_StatusTypeDef (*SetTimestamp)(uint32_t timestamp);
} HW_RTC_InterfaceTypeDef;

typedef struct {
    HW_DHT11_InterfaceTypeDef DHT11;
    HW_MPU6050_InterfaceTypeDef MPU6050;
    HW_ESP8266_InterfaceTypeDef ESP8266;
    HW_RTC_InterfaceTypeDef RealTimeClock;
} HW_InterfaceTypeDef;

extern HW_InterfaceTypeDef HW_Interface;

#endif
