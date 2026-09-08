#ifndef __SYS_DATA_H
#define __SYS_DATA_H
#include "sys_defs.h"
#include <stdint.h>
#include "time_convert.h"
#include "stdbool.h"
#include "net_wifi.h"
// ========== 1. 数据结构定义 ==========

typedef enum {
    SYS_NONE_UPDATE,
    SYS_WIFI_UPDATE,
    SYS_TIME_UPDATE,
    SYS_DATE_UPDATE,
    SYS_TIME_SECOND_UPDATE,
    SYS_WEEKDAY_UPDATE,
    SYS_ENV_UPDATE,
    SYS_ANGLE_UPDATE,
    SYS_WEATHER_UPDATE,
    SYS_THRESHOLD_UPDATE,
    SYS_VOLUME_UPDATE,
    SYS_BACKGROUND_COLOR_UPDATE,
} SYS_DataEventType_t;

typedef enum {
    WEATHER_CODE_SUNNY,
    WEATHER_CODE_CLOUDY,
    WEATHER_CODE_RAINY,
    WEATHER_CODE_OVERCAST,
    WEATHER_CODE_SNOWY,
    WEATHER_CODE_NUM
} WeatherCode_t;

typedef struct {
    int16_t threshold_low;
    int16_t threshold_high;
} ThresholdData_t;

typedef enum {
    THRESHOLD_TYPE_TEMP,
    THRESHOLD_TYPE_HUMI,
    THRESHOLD_TYPE_PITCH,
    THRESHOLD_TYPE_ROLL,
    THRESHOLD_TYPE_YAW,
    THRESHOLD_TYPE_NUM
} ThresholdType_t;

typedef struct {
    wifi_state_t wifi_status; // 0未连接 1已连接
    // 时间
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t weekday; // 0-6
    // 环境
    int16_t temperature; //*10
    int16_t humidity;    //*10
    int16_t pitch;       //*10
    int16_t roll;        //*10
    int16_t yaw;         //*10   MPU6050
    // 天气
    uint8_t weather_code; // 0晴 1云 2雨 3阴 4雪
    int8_t weather_temp;
    int8_t weather_humi;
    int8_t weather_wind;
    char weather_city[12];
    int16_t weather_pressure;
    int16_t weather_update_year;
    uint8_t weather_update_month;
    uint8_t weather_update_day;
    // 系统设置
    ThresholdData_t thresholds[THRESHOLD_TYPE_NUM];
    // 音量
    uint8_t volume;
    // 背景颜色
    uint8_t background_color_code;
    bool onenet_switch;
} SystemGlobalData_t;

typedef struct {
    volatile SystemGlobalData_t data;
    uint8_t reserved[505 - sizeof(SystemGlobalData_t)]; // 468
} FlashStorage_t;

// ========== 2. 核心接口 ==========
SYS_StatusTypeDef SYS_DATA_Init(void); // 初始化（从Storage恢复）
SYS_StatusTypeDef SYS_DATA_Save(void);
// ---- 更新函数（带内部锁，供各任务调用） ----
void SYS_DATA_SetWifiStatus(wifi_state_t status);
void SYS_DATA_SetSysTime(datetime_t datetime);
void SYS_DATA_SetEnv(int16_t temp, int16_t humi);
void SYS_DATA_SetAngle(int16_t pitch, int16_t roll, int16_t yaw);
void SYS_DATA_SetWeather(int16_t weather_code, int8_t temp, int8_t humi, int8_t wind, int16_t pressure);
void SYS_DATA_SetWeatherUpdateDate(uint16_t year, uint8_t month, uint8_t day);
void SYS_DATA_SetWeatherCity(const char *city);
void SYS_DATA_SetThreshold(ThresholdType_t type, int16_t high, uint16_t low);
void SYS_DATA_SetWeatherUpdateDate(uint16_t year, uint8_t month, uint8_t day);
void SYS_DATA_SetVolume(uint8_t vol);
void SYS_DATA_SetBackgroundColor(uint8_t color_code);
void SYS_DATA_SetOnenetSwitch(bool switch_connect);

void SYS_DATA_GetWifiStatus(wifi_state_t *status);
void SYS_DATA_GetSysTime(datetime_t *datetime);
void SYS_DATA_GetEnv(int16_t *temp, int16_t *humi);
void SYS_DATA_GetAngle(int16_t *pitch, int16_t *roll, int16_t *yaw);
void SYS_DATA_GetWeather(int16_t *weather_code, int8_t *temp, int8_t *humi,
                         int8_t *wind, int16_t *pressure);
void SYS_DATA_GetWeatherUpdateDate(uint16_t *year, uint8_t *month, uint8_t *day);
void SYS_DATA_GetThreshold(ThresholdType_t type, int16_t *high, uint16_t *low);
void SYS_DATA_GetVolume(uint8_t *vol);
void SYS_DATA_GetBackgroundColor(uint8_t *color_code);
void SYS_DATA_GetOnenetSwitch(bool *switch_connect);
// ---- 读取快照（供UI任务使用） ----
void SYS_DATA_GetSnapshot(SystemGlobalData_t *out);
#endif