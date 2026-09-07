#include "user_sensorDataUpdateTask.h"
#include "debug_func.h"

#include "HWDataAccess.h"

#include "driver_mpu6050.h"
#include "stm32f1xx_hal.h"
#include "debug_func.h"
#include "time_convert.h"
#include "sys_data.h"
#define HW_UPDATE_TASK_PERIOD_MS 20

typedef struct {
    int16_t pitch;
    int16_t roll;
    int16_t yaw;
} angle_data_t;
typedef struct {
    int16_t temp;
    int16_t humi;
} env_data_t;

extern osMessageQueueId_t xCmdDisplayQueue;
static void time_display_update(const datetime_t last_time, const datetime_t current_time);
static void angle_display_update(const angle_data_t last_angle_data, const angle_data_t current_angle_data);
static void evnt_display_update(const env_data_t last_env_data, const env_data_t current_env_data);

/**
 * @brief Sensor data update task
 * @param pvParameters
 * @retval void
 */
void sensorDataUpdateTask(void *pvParameters) {
    // osDelay(2000); // Delay for 1 second
    env_data_t env_data, last_env_data;
    float pitch_f, roll_f, yaw_f;
    angle_data_t angle_data, last_angle_data;
    static uint16_t time_counter = 0;
    uint32_t last_print_time = 0;
    datetime_t last_datetime;
    datetime_t current_datetime;
    int ret;
    int16_t AccX, AccY, AccZ, GyroX, GyroY, GyroZ;
    while (1) {
        //  TODO: 要不要状态机轮流读取
        uint32_t tick = osKernelGetTickCount();
        if (time_counter % (HW_Interface.DHT11.update_time / HW_UPDATE_TASK_PERIOD_MS) == 0) {
            HW_Interface.DHT11.data_status = HW_Interface.DHT11.GetHumiTemp(&env_data.humi, &env_data.temp);
            if (HW_Interface.DHT11.data_status == SYS_OK) {
                SYS_DATA_SetEnv(env_data.temp, env_data.humi);
                evnt_display_update(last_env_data, env_data);
                last_env_data = env_data;
            } else {
                HW_Interface.DHT11.Init();
            }
        }
        if (!HW_Interface.MPU6050.ConnectionError) {
            HW_Interface.MPU6050.data_status = HW_Interface.MPU6050.GetAngle(&pitch_f, &roll_f, &yaw_f);
            if (time_counter % (HW_Interface.MPU6050.update_time / HW_UPDATE_TASK_PERIOD_MS) == 0) {
                if (HW_Interface.MPU6050.data_status == SYS_OK) {
                    HW_Interface.MPU6050.AngleFloatToInt(pitch_f, roll_f, yaw_f, &angle_data.pitch, &angle_data.roll, &angle_data.yaw); // Convert float to int
                    SYS_DATA_SetAngle(angle_data.pitch, angle_data.roll, angle_data.yaw);
                    angle_display_update(last_angle_data, angle_data);
                    last_angle_data = angle_data;
                } else {
                }
            }
        }
        if (time_counter % (HW_Interface.RealTimeClock.update_time / HW_UPDATE_TASK_PERIOD_MS) == 0) {
            HW_Interface.RealTimeClock.data_status = HW_Interface.RealTimeClock.SetTimedata(&current_datetime);
            if (HW_Interface.RealTimeClock.data_status == SYS_OK) {
                SYS_DATA_SetSysTime(current_datetime);
                time_display_update(last_datetime, current_datetime);
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

void time_display_update(const datetime_t last_time, const datetime_t current_time) {
    SYS_DataEventType_t event = SYS_NONE_UPDATE;
    if (last_time.year != current_time.year || last_time.month != current_time.month || last_time.day != current_time.day) {
        event = SYS_DATE_UPDATE;
        osMessageQueuePut(xCmdDisplayQueue, &event, 0, osWaitForever);
    }
    if (last_time.hour != current_time.hour || last_time.minute != current_time.minute) {
        event = SYS_TIME_UPDATE;
        osMessageQueuePut(xCmdDisplayQueue, &event, 0, osWaitForever);
    }
    if (last_time.second != current_time.second) {
        event = SYS_TIME_SECOND_UPDATE;
        osMessageQueuePut(xCmdDisplayQueue, &event, 0, osWaitForever);
    }
    if (last_time.weekday != current_time.weekday) {
        event = SYS_WEEKDAY_UPDATE;
        osMessageQueuePut(xCmdDisplayQueue, &event, 0, osWaitForever);
    }
}

static void angle_display_update(const angle_data_t last_angle_data, const angle_data_t current_angle_data) {
    SYS_DataEventType_t event;
    if (last_angle_data.pitch != current_angle_data.pitch || last_angle_data.roll != current_angle_data.roll || last_angle_data.yaw != current_angle_data.yaw) {
        event = SYS_ANGLE_UPDATE;
        osMessageQueuePut(xCmdDisplayQueue, &event, 0, osWaitForever);
    }
}

static void evnt_display_update(const env_data_t last_env_data, const env_data_t current_env_data) {
    SYS_DataEventType_t event;
    if (last_env_data.temp != current_env_data.temp || last_env_data.humi != current_env_data.humi) {
        event = SYS_ENV_UPDATE;
        osMessageQueuePut(xCmdDisplayQueue, &event, 0, osWaitForever);
    }
}