#include "user_sensorDataUpdateTask.h"
#include "dht11_driver.h"
#include "SEGGER_RTT.h"

/**
 * @brief Sensor data update task
 * @param pvParameters
 * @retval void
 */
void sensorDataUpdateTask(void *pvParameters) {
    osDelay(2000); // Delay for 1 second
    float temp, humi;
    while (1) {
        if (DHT11_Read(&humi, &temp) == DHT11_OK) {
        } else {
            SEGGER_RTT_printf(0, "DHT11 read failed\n");
            DHT11_Init(); // Reinitialize DHT11
        }
        osDelay(1000); // Delay for 1 second
    }
}