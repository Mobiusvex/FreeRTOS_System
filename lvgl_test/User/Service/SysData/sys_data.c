// sys_data.c
#include "sys_data.h"
#include "sys_storage.h"
#include "cmsis_os2.h"
#include <string.h>
// ========== 静态变量 ==========
static FlashStorage_t g_flashStorage;
static volatile SystemGlobalData_t *const g_sysData = &g_flashStorage.data;
const ThresholdData_t thresholds_range[THRESHOLD_TYPE_NUM] = {{-100, 100}, {0, 100}, {-180, 180}, {-180, 180}, {-180, 180}};

static void sys_data_set_init(void);
/**
 * @brief 初始化系统数据
 * @return SYS_OK 成功，SYS_ERROR 失败
 * @details 初始化系统数据，从 Flash 恢复数据。如果 Flash 恢复失败，将数据清空并返回错误码。否则返回成功码。
 */
SYS_StatusTypeDef SYS_DATA_Init(void) {
    // 此处调用 SYS_Storage_Load 从 Flash 恢复数据
    if (SYS_Storage_Load(&g_flashStorage, sizeof(g_flashStorage)) != SYS_OK) {
        memset(&g_flashStorage, 0, sizeof(g_flashStorage));
        sys_data_set_init();
        return SYS_ERROR; // 初始化失败，返回错误码
    }
    sys_data_set_init();
    return SYS_OK;
}

/**
 * @brief 保存系统数据
 */
SYS_StatusTypeDef SYS_DATA_Save(void) {
    // 此处调用 SYS_Storage_Load 从 Flash 恢复数据
    if (SYS_Storage_Save(&g_flashStorage, sizeof(g_flashStorage)) != SYS_OK) {
        return SYS_ERROR; // 初始化失败，返回错误码
    }
    return SYS_OK;
}

// ============================================================
//                   Set 函数实现
// ============================================================

static void sys_data_set_init(void) {
    if (g_sysData->thresholds[THRESHOLD_TYPE_TEMP].threshold_high == g_sysData->thresholds[THRESHOLD_TYPE_TEMP].threshold_low) {
        SYS_DATA_SetThreshold(THRESHOLD_TYPE_TEMP, 40, 20);
    }
    if (g_sysData->thresholds[THRESHOLD_TYPE_HUMI].threshold_high == g_sysData->thresholds[THRESHOLD_TYPE_HUMI].threshold_low) {
        SYS_DATA_SetThreshold(THRESHOLD_TYPE_HUMI, 80, 20);
    }
    if (g_sysData->thresholds[THRESHOLD_TYPE_PITCH].threshold_high == g_sysData->thresholds[THRESHOLD_TYPE_PITCH].threshold_low) {
        SYS_DATA_SetThreshold(THRESHOLD_TYPE_PITCH, 2, -2);
    }
    if (g_sysData->thresholds[THRESHOLD_TYPE_ROLL].threshold_high == g_sysData->thresholds[THRESHOLD_TYPE_ROLL].threshold_low) {
        SYS_DATA_SetThreshold(THRESHOLD_TYPE_ROLL, 2, -2);
    }
    if (g_sysData->thresholds[THRESHOLD_TYPE_YAW].threshold_high == g_sysData->thresholds[THRESHOLD_TYPE_YAW].threshold_low) {
        SYS_DATA_SetThreshold(THRESHOLD_TYPE_YAW, 2, -2);
    }

    if (g_sysData->volume == 0) {
        SYS_DATA_SetVolume(50);
    }
}

void SYS_DATA_SetWifiStatus(wifi_state_t status) {
    uint32_t lock_state = osKernelLock();
    g_sysData->wifi_status = status;
    osKernelRestoreLock(lock_state);
}

/**
 * @brief 设置系统时间
 * @param datetime 指向 datetime_t 结构体的指针
 */
void SYS_DATA_SetSysTime(const datetime_t datetime) {
    uint32_t lock_state = osKernelLock();
    g_sysData->year = datetime.year;
    g_sysData->month = datetime.month;
    g_sysData->day = datetime.day;
    g_sysData->hour = datetime.hour;
    g_sysData->minute = datetime.minute;
    g_sysData->second = datetime.second;
    if (datetime.weekday <= 0 && datetime.weekday > 7) {
        g_sysData->weekday = 1;
    } else {
        g_sysData->weekday = datetime.weekday;
    }
    osKernelRestoreLock(lock_state);
}

/**
 * @brief 设置环境数据
 * @param temp 温度
 * @param humi 湿度
 */
void SYS_DATA_SetEnv(int16_t temp, int16_t humi) {
    uint32_t lock_state = osKernelLock();
    g_sysData->temperature = temp;
    g_sysData->humidity = humi;

    osKernelRestoreLock(lock_state);
}

/**
 * @brief 设置角度数据
 */
void SYS_DATA_SetAngle(int16_t pitch, int16_t roll, int16_t yaw) {
    uint32_t lock_state = osKernelLock();
    g_sysData->pitch = pitch;
    g_sysData->roll = roll;
    g_sysData->yaw = yaw;

    osKernelRestoreLock(lock_state);
}

/**
 * @brief 设置天气数据
 */
void SYS_DATA_SetWeather(int16_t weather_code, int8_t temp, int8_t humi, int8_t wind, int16_t pressure) {
    uint32_t lock_state = osKernelLock();
    if (weather_code < 0 || weather_code >= WEATHER_CODE_NUM) {
        g_sysData->weather_code = 0;
    } else {
        g_sysData->weather_code = weather_code; // 注意原实现误写为 code，现改正
    }
    g_sysData->weather_temp = temp;
    g_sysData->weather_humi = humi;
    g_sysData->weather_wind = wind;
    g_sysData->weather_pressure = pressure;

    osKernelRestoreLock(lock_state);
}

void SYS_DATA_SetWeatherCity(const char *city) {
    uint32_t lock_state = osKernelLock();
    strncpy(g_sysData->weather_city, city, sizeof(g_sysData->weather_city) - 1);
    g_sysData->weather_city[sizeof(g_sysData->weather_city) - 1] = '\0'; // 确保字符串以空字符结束
    osKernelRestoreLock(lock_state);
}

/**
 * @brief 设置天气更新时间
 * @param year 年份
 * @param month 月份
 * @param day 日期
 */
void SYS_DATA_SetWeatherUpdateDate(uint16_t year, uint8_t month, uint8_t day) {
    uint32_t lock_state = osKernelLock();
    g_sysData->weather_update_year = year;
    g_sysData->weather_update_month = month;
    g_sysData->weather_update_day = day;

    osKernelRestoreLock(lock_state);
}

/**
 * @brief 设置阈值
 * @param type 阈值类型
 * @param high 阈值上限
 * @param low 阈值下限
 */
void SYS_DATA_SetThreshold(ThresholdType_t type, int16_t high, uint16_t low) {
    uint32_t lock_state = osKernelLock();
    ThresholdData_t *thresholds = g_sysData->thresholds;
    if (type >= 0 && type < THRESHOLD_TYPE_NUM) {
        thresholds[type].threshold_high = high;
        thresholds[type].threshold_low = low;

        if (thresholds[type].threshold_high > thresholds_range[type].threshold_high) {
            thresholds[type].threshold_high = thresholds_range[type].threshold_high;
        }
        if (thresholds[type].threshold_low < thresholds_range[type].threshold_low) {
            thresholds[type].threshold_low = thresholds_range[type].threshold_low;
        }
    }
    osKernelRestoreLock(lock_state);
}

/**
 * @brief 设置音量
 */
void SYS_DATA_SetVolume(uint8_t vol) {
    uint32_t lock_state = osKernelLock();
    g_sysData->volume = vol;

    osKernelRestoreLock(lock_state);
}

/**
 * @brief 设置背景颜色
 */
void SYS_DATA_SetBackgroundColor(uint8_t color_code) {
    uint32_t lock_state = osKernelLock();
    g_sysData->background_color_code = color_code;

    osKernelRestoreLock(lock_state);
}

/**
 * @brief 设置是否连接 OneNet
 * @param connect 连接状态
 */
void SYS_DATA_SetOnenetSwitch(bool switch_connect) {
    uint32_t lock_state = osKernelLock();
    g_sysData->onenet_switch = switch_connect;
    osKernelRestoreLock(lock_state);
}
// ============================================================
//                   Get 函数实现（单值读取）
// ============================================================

void SYS_DATA_GetWifiStatus(wifi_state_t *status) {
    if (status) {
        uint32_t lock_state = osKernelLock(); // 加锁
        *status = g_sysData->wifi_status;
        osKernelRestoreLock(lock_state); // 解锁
    }
}

/**
 * @brief 获取系统时间
 */
void SYS_DATA_GetSysTime(datetime_t *datetime) {
    if (datetime) {
        uint32_t lock_state = osKernelLock(); // 加锁
        datetime->year = g_sysData->year;
        datetime->month = g_sysData->month;
        datetime->day = g_sysData->day;
        datetime->hour = g_sysData->hour;
        datetime->minute = g_sysData->minute;
        datetime->second = g_sysData->second;
        osKernelRestoreLock(lock_state); // 解锁
    }
}

/**
 * @brief 获取环境数据
 */
void SYS_DATA_GetEnv(int16_t *temp, int16_t *humi) {
    uint32_t lock_state = osKernelLock();
    *temp = g_sysData->temperature;
    *humi = g_sysData->humidity;
    osKernelRestoreLock(lock_state);
}

/**
 * @brief 获取角度数据
 */
void SYS_DATA_GetAngle(int16_t *pitch, int16_t *roll, int16_t *yaw) {
    uint32_t lock_state = osKernelLock();
    *pitch = g_sysData->pitch;
    *roll = g_sysData->roll;
    *yaw = g_sysData->yaw;
    osKernelRestoreLock(lock_state);
}

/**
 * @brief 获取天气数据
 */
void SYS_DATA_GetWeather(int16_t *weather_code, int8_t *temp, int8_t *humi,
                         int8_t *wind, int16_t *pressure) {
    uint32_t lock_state = osKernelLock();
    *weather_code = g_sysData->weather_code;
    *temp = g_sysData->weather_temp;
    *humi = g_sysData->weather_humi;
    *wind = g_sysData->weather_wind;
    *pressure = g_sysData->weather_pressure;
    osKernelRestoreLock(lock_state);
}

/**
 * @brief 获取天气更新时间
 */
void SYS_DATA_GetWeatherUpdateDate(uint16_t *year, uint8_t *month, uint8_t *day) {
    uint32_t lock_state = osKernelLock();
    *year = g_sysData->weather_update_year;
    *month = g_sysData->weather_update_month;
    *day = g_sysData->weather_update_day;
    osKernelRestoreLock(lock_state);
}

/**
 * @brief 获取阈值
 * @param type 阈值类型
 * @param high 阈值上限
 * @param low 阈值下限
 */
void SYS_DATA_GetThreshold(ThresholdType_t type, int16_t *high, uint16_t *low) {
    uint32_t lock_state = osKernelLock();
    ThresholdData_t *thresholds = g_sysData->thresholds;
    if (type >= 0 && type < THRESHOLD_TYPE_NUM) {
        *high = thresholds[type].threshold_high;
        *low = thresholds[type].threshold_low;
    } else {
        *high = 0;
        *low = 0;
    }
    osKernelRestoreLock(lock_state);
}

/**
 * @brief 获取音量
 */
void SYS_DATA_GetVolume(uint8_t *vol) {
    uint32_t lock_state = osKernelLock();
    *vol = g_sysData->volume;
    osKernelRestoreLock(lock_state);
}

/**
 * @brief 获取背景颜色
 */
void SYS_DATA_GetBackgroundColor(uint8_t *color_code) {
    uint32_t lock_state = osKernelLock();
    *color_code = g_sysData->background_color_code;
    osKernelRestoreLock(lock_state);
}

/**
 * @brief 获取是否连接 OneNet
 */
void SYS_DATA_GetOnenetSwitch(bool *switch_connect) {
    uint32_t lock_state = osKernelLock();
    *switch_connect = g_sysData->onenet_switch;
    osKernelRestoreLock(lock_state);
}

void SYS_DATA_GetSnapshot(SystemGlobalData_t *out) {
    uint32_t lock_state = osKernelLock();
    memcpy(out, g_sysData, sizeof(SystemGlobalData_t)); // 复制数据到 out
    osKernelRestoreLock(lock_state);
}