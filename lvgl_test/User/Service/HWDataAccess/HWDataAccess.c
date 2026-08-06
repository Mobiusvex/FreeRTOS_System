#include "HWDataAccess.h"

#include "dht11_driver.h"

/**
 * @brief DHT11传感器初始化
 * @retval 初始化成功返回HW_STATUS_OK，初始化失败返回HW_STATUS_ERROR
 */
uint8_t HW_DHT11_Init(void) {
    // Initialize DHT11 sensor
    DHT11_Init();
    return HW_STATUS_OK;
}
/**
 * @brief 读取DHT11传感器的湿度和温度
 * @param humi 指向存储湿度值的指针
 * @param temp 指向存储温度值的指针
 * @retval 读取成功返回HW_STATUS_OK，读取失败返回HW_STATUS_ERROR
 */
uint8_t HW_DHT11_Get_Humi_Temp(float *humi, float *temp) {
    if (DHT11_Read(humi, temp) == DHT11_OK) {
        return HW_STATUS_OK;
    } else {
        return HW_STATUS_ERROR;
    }
}

HW_InterfaceTypeDef HW_Interface = {
    .DHT11 = {
        .ConnectionError = 1,
        .humidity = 67,
        .temperature = 26,
        .Init = HW_DHT11_Init,
        .GetHumiTemp = HW_DHT11_Get_Humi_Temp},
};