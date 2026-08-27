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
    // HACK： 要不要加volatile保护
    float pitch_angle;
    float roll_angle;
    float yaw_angle;

    SYS_StatusTypeDef (*Init)(void);
    SYS_StatusTypeDef (*GetAngle)(float *pitch, float *roll, float *yaw);
} HW_MPU6050_InterfaceTypeDef;

typedef struct
{
    uint16_t head;
    uint16_t tail;
} HW_Uart_ReceiverTypeDef;
// NOTE:接收任务接收数据，将数据放进缓冲区，写入队列（接收长度）
// NOTE:esp8266通信任务从队列读取数据，判断数据类型，调用相应的接口函数
// NOTE:esp8266通信任务的接口函数如下：
// NOTE:接口函数传入的状态机参数有：数据缓冲区（指针），数据长度，数据类型
// NOTE:1.AT指令发送与接收：状态机判断当前步骤，进行AT指令接收判定
// NOTE:2.控制指令发送与接收：状态机判断当前步骤，进行控制指令接收判定
// NOTE:3.天气指令发送与接收：状态机判断当前步骤，进行天气指令接收判定
// NOTE:4.时间指令发送与接收：状态机判断当前步骤，进行时间指令接收判定
// NOTE:5.数据处理：将接收到的数据进行处理，更新数据状态，更新数据缓冲区

typedef struct
{
    uint8_t ConnectionError;
    uint16_t update_time;
    SYS_StatusTypeDef data_status;
    SYS_StatusTypeDef (*Init)(void);
    SYS_StatusTypeDef (*Reset)(void);
} HW_ESP8266_InterfaceTypeDef;

typedef struct
{
    SYS_StatusTypeDef data_status;
    uint16_t update_time;
    SYS_StatusTypeDef (*GetTimeString)(char *, uint8_t);
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
