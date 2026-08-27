#ifndef TIME_CONVERT_H
#define TIME_CONVERT_H
#include <stdint.h>

typedef struct {
    uint8_t weekday; // 1-7 (周一=1, 周日=7，符合ISO标准)
    uint16_t year;   // 完整年份，如 2026
    uint8_t month;   // 1-12
    uint8_t day;     // 1-31
    uint8_t hour;    // 0-23
    uint8_t minute;  // 0-59
    uint8_t second;  // 0-59
} datetime_t;

void timestamp_to_datetime(uint32_t timestamp, datetime_t *dt);
uint32_t datetime_to_timestamp(const datetime_t *dt);

#endif // TIME_CONVERT_H