#include "user_sensorDataUpdateTask.h"
#include "SEGGER_RTT.h"

#include "HWDataAccess.h"

#include "driver_mpu6050.h"
#include "stm32f1xx_hal.h"

#define HW_UPDATE_TASK_PERIOD_MS 20
/**
 * @brief Sensor data update task
 * @param pvParameters
 * @retval void
 */
void sensorDataUpdateTask(void *pvParameters) {
    // osDelay(2000); // Delay for 1 second
    float temp, humi;
    float pitch, roll, yaw;
    static uint16_t time_counter = 0;

    int ret;
    int16_t AccX, AccY, AccZ, GyroX, GyroY, GyroZ;
    while (1) {
        // TODO: 要不要状态机轮流读取
        uint32_t tick = osKernelGetTickCount();
        if (time_counter % (HW_Interface.DHT11.update_time / HW_UPDATE_TASK_PERIOD_MS) == 0) {
            HW_Interface.DHT11.data_status = HW_Interface.DHT11.GetHumiTemp(&humi, &temp);
            if (HW_Interface.DHT11.data_status == SYS_OK) {
                HW_Interface.DHT11.humidity = humi;
                HW_Interface.DHT11.temperature = temp;
                SEGGER_RTT_printf(0, "Temperature: %f°C, Humidity: %f%%\n", temp, humi); // Print sensor data to RTT
            } else {
                SEGGER_RTT_printf(0, "DHT11 read failed\n");
                HW_Interface.DHT11.Init(); // Reinitialize DHT11
            }
        }
        if (!HW_Interface.MPU6050.ConnectionError) {
            HW_Interface.MPU6050.data_status = HW_Interface.MPU6050.GetAngle(&pitch, &roll, &yaw);
            if (time_counter % (HW_Interface.MPU6050.update_time / HW_UPDATE_TASK_PERIOD_MS) == 0) {
                if (HW_Interface.MPU6050.data_status == SYS_OK) {
                    HW_Interface.MPU6050.pitch_angle = pitch;
                    HW_Interface.MPU6050.roll_angle = roll;
                    HW_Interface.MPU6050.yaw_angle = yaw; // Update MPU6050 angles
                    SEGGER_RTT_printf(0, "Pitch: %f, Roll: %f, Yaw: %f\n", pitch, roll, yaw);
                } else {
                }
            }
        }
        // HACK: 任务周期非严格20ms，DHT11每次读取20ms，6050每次读取13ms
        // SEGGER_RTT_printf(0, "Time: %dms\n", osKernelGetTickCount() - tick); // Print current time to RTT
        tick += HW_UPDATE_TASK_PERIOD_MS;
        osDelayUntil(tick); // 20ms
        time_counter++;
        if (time_counter > 1000) {
            time_counter = 0;
        }
    }
}