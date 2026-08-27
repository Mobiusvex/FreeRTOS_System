#ifndef NET_MANAGER_H
#define NET_MANAGER_H
#include <stdint.h>

typedef enum {
    ESP8266_APP_STATE_WIFI_CONNECT, // 连接WiFi
    ESP8266_APP_STATE_GET_TIME,     // 获取时间（开机先取时间）
    ESP8266_APP_STATE_GET_WEATHER,  // 获取天气（后续每小时）
    ESP8266_APP_STATE_UPLOAD_DATA,  // 周期上报或立即上报
    ESP8266_APP_STATE_IDLE,         // 空闲（等待定时器或用户指令）
    ESP8266_APP_INVALID_CMD,        // 无效命令
    ESP8266_APP_STATE_ERROR         // 全局致命错误（重启模块）
} esp8266_app_state_t;

// 命令枚举
typedef enum {
    ESP8266_CMD_NONE = 0,
    ESP8266_CMD_REBOOT_WIFI,   // 重启WiFi
    ESP8266_CMD_FETCH_WEATHER, // 获取天气
    ESP8266_CMD_FETCH_TIME,    // 获取时间
    ESP8266_CMD_UPLOAD_DATA,   // 立即上报（超范围触发）
} esp8266_cmd_t;

void ESP8266_APP_Init(void);
esp8266_app_state_t ESP8266_APP_Run(esp8266_cmd_t cmd, uint8_t *rx_buf, uint32_t rx_buf_size);

#endif // NET_MANAGER_H