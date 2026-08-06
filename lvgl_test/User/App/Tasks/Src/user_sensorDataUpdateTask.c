#include "user_sensorDataUpdateTask.h"
#include "SEGGER_RTT.h"

#include "HWDataAccess.h"

/**
 * @brief Sensor data update task
 * @param pvParameters
 * @retval void
 */
void sensorDataUpdateTask(void *pvParameters) {
    osDelay(2000); // Delay for 1 second
    float temp, humi;
    while (1) {
        if (HW_Interface.DHT11.GetHumiTemp(&humi, &temp) == HW_STATUS_OK) {
            HW_Interface.DHT11.humidity = humi;
            HW_Interface.DHT11.temperature = temp;
            // SEGGER_RTT_printf(0, "Temperature: %f°C, Humidity: %f%%\n", temp, humi); // Print sensor data to RTT
        } else {
            // SEGGER_RTT_printf(0, "DHT11 read failed\n");
            HW_Interface.DHT11.Init(); // Reinitialize DHT11
        }
        osDelay(1000); // Delay for 1 second
    }
}