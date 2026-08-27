#include "user_ESP8266CommTask.h"
#include "HWDataAccess.h"
#include "net_cloud.h"
#include "net_time.h"
#include "net_weather.h"
#include "net_wifi.h"
#include "FreeRTOS.h"
#include "stream_buffer.h"
#include "debug_func.h"
#include "net_manager.h"
#include "SEGGER_RTT.h"
#include "string.h"

#define ESP8266_WEATHER_UPDATE_INTERVAL 40000
#define ESP8266_TIME_UPDATE_INTERVAL 20000
#define ESP8266_UPLOAD_DATA_INTERVAL 30000
#define ESP8266_CMD_MANAGE_INTERVAL 100

#define ESP8266_ERROR_REBOOT_INTERVAL 20000

#define ESP8266_TASK_PERIOD 10

extern StreamBufferHandle_t xESP8266StreamBuffer;
extern osMessageQueueId_t xESP8266CmdQueue;

#define ESP8266_TASK_BUFFER_SIZE 512

void user_ESP8266CommTask(void *pvParameters) {
    // 创建buffer
    static uint8_t buffer[ESP8266_TASK_BUFFER_SIZE];
    uint32_t buf_len = 0;
    uint32_t time_count = 0;
    uint32_t error_count = 0;
    esp8266_app_state_t state = ESP8266_APP_STATE_IDLE;
    esp8266_cmd_t next_cmd = ESP8266_CMD_REBOOT_WIFI;

    net_wifi_init();
    net_weather_init();
    net_time_init();
    while (1) {
        uint32_t tick = osKernelGetTickCount();

        // 没有要执行的命令，从队列中获取命令
        if (next_cmd == ESP8266_CMD_NONE) {
            osMessageQueueGet(xESP8266CmdQueue, &next_cmd, 0, 0);
        }

        buf_len = xStreamBufferReceive(xESP8266StreamBuffer, buffer, 512, 0);
        // 接收到ESP8266发来的数据，立即执行
        if (buf_len > 0) {
            state = ESP8266_APP_Run(next_cmd, buffer, buf_len);
            memset(buffer, 0, ESP8266_TASK_BUFFER_SIZE);
            // ESP8266空闲状态，本次指令执行完毕，清本次指令
            if (state == ESP8266_APP_STATE_IDLE) {
                next_cmd = ESP8266_CMD_NONE;
            }
        } else {
            // 没有接收到ESP8266发来的数据，定期执行以进行状态处理
            if (time_count % (ESP8266_CMD_MANAGE_INTERVAL / ESP8266_TASK_PERIOD) == 0) {
                state = ESP8266_APP_Run(next_cmd, buffer, buf_len);
                // ESP8266空闲状态，本次指令执行完毕，清本次指令
                if (state == ESP8266_APP_STATE_IDLE) {
                    next_cmd = ESP8266_CMD_NONE;
                }
            }
        }

        if (state == ESP8266_APP_STATE_ERROR) {
            error_count++;
            // ESP8266错误状态保持一段时间则复位ESP8266
            if ((error_count % (ESP8266_ERROR_REBOOT_INTERVAL / ESP8266_TASK_PERIOD)) == 0) {
                HW_Interface.ESP8266.Reset(); // 重启ESP8266
                next_cmd = ESP8266_CMD_REBOOT_WIFI;
                SEGGER_RTT_printf(0, "ESP8266 RESET----------\n");
                error_count = 0;
            }
        } else {
            error_count = 0;
        }

        // 测试代码，测试各个模块是否能正常工作--------------------
        if ((time_count % (ESP8266_WEATHER_UPDATE_INTERVAL / ESP8266_TASK_PERIOD)) == 0) {
            if (next_cmd == ESP8266_CMD_NONE) {
                set_weather_city("Shanghai");
                next_cmd = ESP8266_CMD_FETCH_WEATHER;
            }
        } else if ((time_count % (ESP8266_TIME_UPDATE_INTERVAL / ESP8266_TASK_PERIOD)) == 0) {
            if (next_cmd == ESP8266_CMD_NONE) {
                next_cmd = ESP8266_CMD_FETCH_TIME;
            }
        }
        if (time_count >= ESP8266_WEATHER_UPDATE_INTERVAL) {
            time_count = 0;
        }
        //-----------------------------------------------------

        tick += ESP8266_TASK_PERIOD;
        osDelayUntil(tick);
        time_count++;

        // 调试：任务剩余栈空间打印
        static uint32_t last_print_time = 0;
        printTaskStackRemainingCapacity((TaskHandle_t)osThreadGetId(), 10000, &last_print_time);
    }
}