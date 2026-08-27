#include <stdint.h>
#include <stdbool.h>
#include "time_convert.h"

/**
 * @brief 判断是否为闰年
 * @param year 年份
 * @return true 是闰年，false 不是
 */
static bool is_leap_year(uint16_t year) {
    return (year % 400 == 0) || (year % 4 == 0 && year % 100 != 0);
}

/**
 * @brief 获取指定年份的天数（平年365，闰年366）
 */
static uint16_t days_in_year(uint16_t year) {
    return is_leap_year(year) ? 366 : 365;
}

/**
 * @brief 获取指定年份和月份的天数
 */
static uint8_t days_in_month(uint16_t year, uint8_t month) {
    static const uint8_t days[12] = {31, 28, 31, 30, 31, 30,
                                     31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap_year(year))
        return 29;
    return days[month - 1];
}

/**
 * @brief 将Unix时间戳转换为日期时间（UTC）
 * @param timestamp 自2000-01-01 00:00:00 UTC以来的秒数
 * @param dt 输出结构体指针
 */
void timestamp_to_datetime(uint32_t timestamp, datetime_t *dt) {
    uint32_t seconds = timestamp;

    // 1. 计算时分秒
    uint32_t day_seconds = seconds % 86400; // 当天的秒数
    dt->hour = day_seconds / 3600;
    dt->minute = (day_seconds % 3600) / 60;
    dt->second = day_seconds % 60;

    // 2. 计算天数（从2000-01-01开始）
    uint32_t days = seconds / 86400;

    // 3. 推算年份
    uint16_t year = 2000;
    while (days >= days_in_year(year)) {
        days -= days_in_year(year);
        year++;
    }
    dt->year = year;

    // 4. 推算月份和日期
    uint8_t month = 1;
    while (1) {
        uint8_t dim = days_in_month(year, month);
        if (days < dim) break;
        days -= dim;
        month++;
    }
    dt->month = month;
    dt->day = days + 1; // 因为days从0开始
}

/**
 * @brief 将日期时间转换为Unix时间戳（UTC）
 * @param dt 输入日期时间结构体指针
 * @retval 自2000-01-01 00:00:00 UTC以来的秒数
 * @note 年份范围：2000 ~ 2136 (uint32_t可容纳)
 */
uint32_t datetime_to_timestamp(const datetime_t *dt) {
    uint32_t days = 0;

    // 1. 累加2000年到目标年份之前的所有天数
    for (uint16_t y = 2000; y < dt->year; y++) {
        days += days_in_year(y);
    }

    // 2. 累加当年已过月份的天数
    for (uint8_t m = 1; m < dt->month; m++) {
        days += days_in_month(dt->year, m);
    }

    // 3. 加上当月已过天数（注意 day 从1开始，所以减1）
    days += (dt->day - 1);

    // 4. 计算当天的秒数
    uint32_t seconds = dt->hour * 3600 + dt->minute * 60 + dt->second;

    // 5. 总秒数 = 天数 * 86400 + 当天秒数
    return days * 86400 + seconds;
}