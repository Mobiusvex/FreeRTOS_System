#include "HWDataAccess.h"

#include "driver_dht11.h"
#include "driver_mpu6050.h"
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
};
