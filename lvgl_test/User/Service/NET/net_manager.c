#include "net_manager.h"

#include "net_wifi.h"
#include "net_weather.h"
#include "net_cloud.h"
#include "net_time.h"

static esp8266_cmd_t s_pending_cmd = ESP8266_CMD_NONE; // 忙时暂存的指令

/**
 * @brief ESP8266操作管理 主函数
 * @param cmd 指令
 * @param rx_buf 接收缓冲区
 * @param rx_buf_size 接收缓冲区大小
 * @return esp8266_app_state_t 操作状态
 */
esp8266_app_state_t ESP8266_APP_Run(esp8266_cmd_t cmd, uint8_t *rx_buf, uint32_t rx_buf_size) {
    cmd_state_t ret;
    if (cmd == ESP8266_CMD_NONE) {
        return ESP8266_APP_STATE_IDLE;
    }

    switch (cmd) {
    case ESP8266_CMD_REBOOT_WIFI:
        ret = net_wifi_mode_set(rx_buf, rx_buf_size);
        if (ret == CMD_STATE_DONE) {
            return ESP8266_APP_STATE_IDLE;
        } else if (ret == CMD_STATE_TIMEOUT || ret == CMD_STATE_FAIL) {
            return ESP8266_APP_STATE_ERROR;
        }
        return ESP8266_APP_STATE_WIFI_CONNECT;

    case ESP8266_CMD_FETCH_TIME:
        // ret = Time_Get_Process(); // 获取网络时间
        if (ret == CMD_STATE_DONE || ret == CMD_STATE_TIMEOUT || ret == CMD_STATE_FAIL) {
            // 无论成败，进入空闲（等待定时器触发下一次）
            return ESP8266_APP_STATE_IDLE;
        }
        return ESP8266_APP_STATE_GET_TIME;

    case ESP8266_CMD_FETCH_WEATHER:
        ret = net_weather_mode_set(rx_buf, rx_buf_size);
        if (ret == CMD_STATE_DONE || ret == CMD_STATE_TIMEOUT || ret == CMD_STATE_FAIL) {
            // 无论成败，进入空闲（等待定时器触发下一次）
            return ESP8266_APP_STATE_IDLE;
        }
        return ESP8266_APP_STATE_GET_WEATHER;

    case ESP8266_CMD_UPLOAD_DATA:
        // ret = DataUpload_Process(); // 上报温湿度、角度
        if (ret == CMD_STATE_DONE || ret == CMD_STATE_TIMEOUT || ret == CMD_STATE_FAIL) {
            return ESP8266_APP_STATE_IDLE; // 上报完成，回空闲
        }
        return ESP8266_APP_STATE_UPLOAD_DATA;
    default:
        return ESP8266_APP_STATE_ERROR;
    }
}