#include "user_sensorDataUpdateTask.h"
#include "debug_func.h"

#include "HWDataAccess.h"

#include "driver_mpu6050.h"
#include "stm32f1xx_hal.h"
#include "debug_func.h"
#include "time_convert.h"
#include "sys_data.h"
#define HW_UPDATE_TASK_PERIOD_MS 20

extern osMessageQueueId_t xCmdDisplayQueue;
SYS_DataEventType_t time_update_decide(const datetime_t *last_time, const datetime_t *current_time);
/**
 * @brief Sensor data update task
 * @param pvParameters
 * @retval void
 */
void sensorDataUpdateTask(void *pvParameters) {
    // osDelay(2000); // Delay for 1 second
    uint16_t temp, humi;
    float pitch_f, roll_f, yaw_f;
    int16_t pitch, roll, yaw;
    static uint16_t time_counter = 0;
    uint32_t last_print_time = 0;
    datetime_t last_datetime;
    datetime_t current_datetime;
    SYS_DataEventType_t event;
    int ret;
    int16_t AccX, AccY, AccZ, GyroX, GyroY, GyroZ;
    while (1) {
        //  TODO: 要不要状态机轮流读取
        uint32_t tick = osKernelGetTickCount();
        if (time_counter % (HW_Interface.DHT11.update_time / HW_UPDATE_TASK_PERIOD_MS) == 0) {
            HW_Interface.DHT11.data_status = HW_Interface.DHT11.GetHumiTemp(&humi, &temp);
            if (HW_Interface.DHT11.data_status == SYS_OK) {
                SYS_DATA_SetEnv(temp, humi);
                event = SYS_ENV_UPDATE;
                osMessageQueuePut(xCmdDisplayQueue, &event, 0, osWaitForever);
            } else {
                HW_Interface.DHT11.Init();
            }
        }
        if (!HW_Interface.MPU6050.ConnectionError) {
            HW_Interface.MPU6050.data_status = HW_Interface.MPU6050.GetAngle(&pitch_f, &roll_f, &yaw_f);
            if (time_counter % (HW_Interface.MPU6050.update_time / HW_UPDATE_TASK_PERIOD_MS) == 0) {
                if (HW_Interface.MPU6050.data_status == SYS_OK) {
                    HW_Interface.MPU6050.AngleFloatToInt(pitch_f, roll_f, yaw_f, &pitch, &roll, &yaw); // Convert float to int
                    SYS_DATA_SetAngle(pitch, roll, yaw);
                    event = SYS_ANGLE_UPDATE;
                    osMessageQueuePut(xCmdDisplayQueue, &event, 0, osWaitForever);
                } else {
                }
            }
        }
        if (time_counter % (HW_Interface.RealTimeClock.update_time / HW_UPDATE_TASK_PERIOD_MS) == 0) {
            HW_Interface.RealTimeClock.data_status = HW_Interface.RealTimeClock.SetTimedata(&current_datetime);
            if (HW_Interface.RealTimeClock.data_status == SYS_OK) {
                SYS_DATA_SetSysTime(current_datetime);
                event = time_update_decide(&last_datetime, &current_datetime);
                osMessageQueuePut(xCmdDisplayQueue, &event, 0, osWaitForever);
                last_datetime = current_datetime;
            }
        }
        // HACK: 任务周期非严格20ms，DHT11每次读取20ms，6050每次读取13ms
        // RTT_PRINTF("Time: %dms\n", osKernelGetTickCount() - tick); // Print current time to RTT
        tick += HW_UPDATE_TASK_PERIOD_MS;
        osDelayUntil(tick); // 20ms
        time_counter++;
        if (time_counter > 1000) {
            time_counter = 0;
        }
        // 调试：任务剩余栈空间打印
        static uint32_t last_print_time = 0;
    }
}

SYS_DataEventType_t time_update_decide(const datetime_t *last_time, const datetime_t *current_time) {
    if (last_time->year != current_time->year || last_time->month != current_time->month || last_time->day != current_time->day) {
        return SYS_DATE_UPDATE;
    } else if (last_time->hour != current_time->hour || last_time->minute != current_time->minute) {
        return SYS_TIME_UPDATE;
    } else if (last_time->second != current_time->second) {
        return SYS_TIME_SECOND_UPDATE;
    }
    return SYS_NONE_UPDATE;
}