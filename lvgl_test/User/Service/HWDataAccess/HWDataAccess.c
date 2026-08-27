#include "HWDataAccess.h"

#include "driver_dht11.h"
#include "driver_mpu6050.h"
#include "driver_esp8266.h"
#include <string.h>
#include "bsp_rtc.h"

#include "stdio.h"
/**
 * @brief DHT11传感器初始化
 * @retval 初始化成功返回SYS_OK，初始化失败返回SYS_ERROR
 */
SYS_StatusTypeDef HW_DHT11_Init(void) {
    // Initialize DHT11 sensor
    DHT11_Init();
    return SYS_OK;
}
/**
 * @brief 读取DHT11传感器的湿度和温度
 * @param humi 指向存储湿度值的指针
 * @param temp 指向存储温度值的指针
 * @retval 读取成功返回SYS_OK，读取失败返回SYS_ERROR
 */
SYS_StatusTypeDef HW_DHT11_Get_Humi_Temp(float *humi, float *temp) {
    return DHT11_Read(humi, temp);
}

/**
 * @brief MPU6050传感器初始化
 * @param 无
 * @retval 初始化成功返回SYS_OK
 */
SYS_StatusTypeDef HW_MPU6050_Init(void) {
    // Initialize MPU6050 sensor
    return MPU6050_Init();
}

/**
 * @brief 读取MPU6050传感器的角度
 * @param pitch 指向存储俯仰角的指针
 * @param roll 指向存储滚转角的指针
 * @param yaw 指向存储偏航角的指针
 * @retval 读取成功返回SYS_OK
 */
SYS_StatusTypeDef HW_MPU6050_Get_Angle(float *pitch, float *roll, float *yaw) {
    return MPU6050_getAngle(pitch, roll, yaw);
}

/**
 * @brief ESP8266传感器初始化
 * @param 无
 * @retval 初始化成功返回SYS_OK
 */
SYS_StatusTypeDef HW_ESP8266_Init(void) {
    return ESP8266_Init(); // Initialize ESP8266 sensor
}

SYS_StatusTypeDef HW_ESP8266_Reset(void) {
    return ESP8266_Reset(); // Reset ESP8266 sensor
}

/**
 * @brief 获取当前时间字符串
 * @param buffer 指向存储时间字符串的缓冲区
 * @param len 缓冲区长度
 * @retval 获取成功返回SYS_OK，获取失败返回SYS_ERROR
 */
SYS_StatusTypeDef HW_Time_GetString(char *buffer, uint8_t len) {
    datetime_t now;
    if (BSP_RTC_GetTime(&now) == SYS_OK) {
        snprintf(buffer, len, "%04d-%02d-%02d %02d:%02d:%02d",
                 now.year, now.month, now.day, now.hour, now.minute, now.second);
        return SYS_OK;
    } else {
        return SYS_ERROR; // 获取RTC时间失败1787730595597
    }
}

#define TIMESTAMP_TIME_2000_OFFSET 946656000

/**
 * @brief 设置RTC时间
 * @param timestamp_sec 时间戳（秒）
 * @retval 设置成功返回SYS_OK，设置失败返回SYS_ERROR
 */
SYS_StatusTypeDef HW_Time_SetRTC(uint32_t timestamp_sec) {
    return BSP_RTC_SetTimeUnix(timestamp_sec - TIMESTAMP_TIME_2000_OFFSET);
}

HW_InterfaceTypeDef HW_Interface = {
    .DHT11 = {
        .ConnectionError = 1,
        .update_time = 500,
        .data_status = SYS_ERROR,
        .humidity = 67,
        .temperature = 26,
        .Init = HW_DHT11_Init,
        .GetHumiTemp = HW_DHT11_Get_Humi_Temp},
    .MPU6050 = {// MPU6050 sensor
                .ConnectionError = 1,
                .update_time = 500,
                .data_status = SYS_ERROR,
                .pitch_angle = 0,
                .roll_angle = 0,
                .Init = HW_MPU6050_Init,
                .GetAngle = HW_MPU6050_Get_Angle},
    .ESP8266 = {// ESP8266 sensor
                .ConnectionError = 1,
                .update_time = 500,
                .data_status = SYS_ERROR,
                .Init = HW_ESP8266_Init,
                .Reset = HW_ESP8266_Reset},
    .RealTimeClock = {//
                      .update_time = 5000,
                      .GetTimeString = HW_Time_GetString,
                      .SetTimestamp = HW_Time_SetRTC},
};
