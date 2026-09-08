#include "net_manager.h"

#include "net_wifi.h"
#include "net_weather.h"
#include "net_cloud.h"
#include "net_time.h"
#include "net_cloud.h"
#include "HWDataAccess.h"
#include "string.h"
#include "sys_data.h"

extern osMessageQueueId_t xCmdDisplayQueue;

static esp8266_cmd_t s_pending_cmd = ESP8266_CMD_NONE; // 忙时暂存的指令

extern uint32_t net_time_get_time(void);
/**
 * @brief ESP8266操作管理 主函数
 * @param cmd 指令
 * @param rx_buf 接收缓冲区
 * @param rx_buf_size 接收缓冲区大小
 * @return esp8266_app_state_t 操作状态
 */
esp8266_app_state_t ESP8266_APP_Run(esp8266_cmd_t cmd, uint8_t *rx_buf, uint32_t rx_buf_size) {
    cmd_state_t state;
    SYS_DataEventType_t event;
    esp8266_app_state_t main_state = ESP8266_APP_STATE_IDLE;
    static bool is_error = false;
    uint32_t timestamp = 0;

    if (rx_buf_size > 0 && strstr(rx_buf, "MQTTSUBRECV")) {
        cloud_setdata_analysis(rx_buf, rx_buf_size);
        rx_buf_size = 0;
    }
    switch (cmd) {
    case ESP8266_CMD_NONE:
        main_state = ESP8266_APP_STATE_IDLE;
        break;
    case ESP8266_CMD_REBOOT_WIFI:
        state = net_wifi_mode_set(rx_buf, rx_buf_size);
        main_state = ESP8266_APP_STATE_WIFI_CONNECT;
        break;

    case ESP8266_CMD_FETCH_TIME:
        state = net_time_mode_set(rx_buf, rx_buf_size); // 获取网络时间
        if (state == CMD_STATE_DATA_ANALYSIS_OK) {
            timestamp = net_time_get_time();
            HW_Interface.RealTimeClock.SetTimestamp(timestamp);
        }
        main_state = ESP8266_APP_STATE_GET_TIME; // 获取网络时间完成，回空闲
        break;

    case ESP8266_CMD_FETCH_WEATHER:
        state = net_weather_mode_set(rx_buf, rx_buf_size);
        if (state == CMD_STATE_DATA_ANALYSIS_OK) {
            weather_sys_data_update();
            event = SYS_WEATHER_UPDATE; // 更新天气数据
            osMessageQueuePut(xCmdDisplayQueue, &event, 0, 50);
        }
        main_state = ESP8266_APP_STATE_GET_WEATHER;
        break;

    case ESP8266_CMD_UPLOAD_DATA:
        state = net_cloud_mode_set(rx_buf, rx_buf_size); // 上报温湿度、角度
        main_state = ESP8266_APP_STATE_UPLOAD_DATA;
        break;
    default:
        main_state = ESP8266_APP_INVALID_CMD;
    }
    // 发生错误后状态设置为错误，直到收到一次成功指令才解除错误标志
    if (state == CMD_STATE_DONE) {
        is_error = false;
        main_state = ESP8266_APP_STATE_IDLE; // 操作完成，回空闲
        SYS_DATA_SetWifiStatus(WIFI_STATE_CONNECTED);
        event = SYS_WIFI_UPDATE;
        osMessageQueuePut(xCmdDisplayQueue, &event, 0, 50);
    } else if (state == CMD_STATE_FAIL || state == CMD_STATE_TIMEOUT) {
        is_error = true;
        SYS_DATA_SetWifiStatus(WIFI_STATE_DISCONNECTED);
        event = SYS_WIFI_UPDATE;
        osMessageQueuePut(xCmdDisplayQueue, &event, 0, 50);
    }
    if (is_error) {
        main_state = ESP8266_APP_STATE_ERROR;
    }
    return main_state;
}