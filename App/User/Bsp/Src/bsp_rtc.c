// bsp_rtc.c
#include "bsp_rtc.h"
#include "time.h"

extern RTC_HandleTypeDef hrtc; // CubeMX生成的句柄

/**
 * @brief 进入RTC初始化模式
 * @param hrtc RTC句柄
 * @retval SYS_OK/ SYS_ERROR/ SYS_INVALID_PARAM
 */
static SYS_StatusTypeDef BSP_RTC_EnterInitMode(RTC_HandleTypeDef *hrtc) {
    uint32_t tickstart = 0U;

    tickstart = HAL_GetTick();
    /* Wait till RTC is in INIT state and if Time out is reached exit */
    while ((hrtc->Instance->CRL & RTC_CRL_RTOFF) == (uint32_t)RESET) {
        if ((HAL_GetTick() - tickstart) > RTC_TIMEOUT_VALUE) {
            return SYS_TIMEOUT;
        }
    }

    /* Disable the write protection for RTC registers */
    __HAL_RTC_WRITEPROTECTION_DISABLE(hrtc);

    return SYS_OK;
}

/**
 * @brief 退出初始化模式
 * @param hrtc RTC句柄
 * @retval SYS_OK/ SYS_ERROR/ SYS_INVALID_PARAM
 */
static SYS_StatusTypeDef BSP_RTC_ExitInitMode(RTC_HandleTypeDef *hrtc) {
    uint32_t tickstart = 0U;

    /* Disable the write protection for RTC registers */
    __HAL_RTC_WRITEPROTECTION_ENABLE(hrtc);

    tickstart = HAL_GetTick();
    /* Wait till RTC is in INIT state and if Time out is reached exit */
    while ((hrtc->Instance->CRL & RTC_CRL_RTOFF) == (uint32_t)RESET) {
        if ((HAL_GetTick() - tickstart) > RTC_TIMEOUT_VALUE) {
            return SYS_TIMEOUT;
        }
    }

    return SYS_OK;
}

/**
 * @brief 读取RTC秒计数器
 * @param hrtc RTC句柄
 * @retval RTC计数器值
 */
uint32_t BSP_RTC_ReadTimeCounter(RTC_HandleTypeDef *hrtc) {
    uint16_t high1 = 0U, high2 = 0U, low = 0U;
    uint32_t timecounter = 0U;

    high1 = READ_REG(hrtc->Instance->CNTH & RTC_CNTH_RTC_CNT);
    low = READ_REG(hrtc->Instance->CNTL & RTC_CNTL_RTC_CNT);
    high2 = READ_REG(hrtc->Instance->CNTH & RTC_CNTH_RTC_CNT);

    if (high1 != high2) {
        /* In this case the counter roll over during reading of CNTL and CNTH registers,
           read again CNTL register then return the counter value */
        timecounter = (((uint32_t)high2 << 16U) | READ_REG(hrtc->Instance->CNTL & RTC_CNTL_RTC_CNT));
    } else {
        /* No counter roll over during reading of CNTL and CNTH registers, counter
           value is equal to first value of CNTL and CNTH */
        timecounter = (((uint32_t)high1 << 16U) | low);
    }

    return timecounter;
}

/**
 * @brief 自定义写入秒计数器（完全自包含，不依赖HAL内部静态函数）
 * @param TimeCounter 自 2000-01-01 00:00:00 以来的秒数
 * @retval SYS_OK/ SYS_ERROR/ SYS_INVALID_PARAM
 */
static SYS_StatusTypeDef BSP_RTC_WriteTimeCounter(RTC_HandleTypeDef *hrtc, uint32_t TimeCounter) {
    HAL_StatusTypeDef status = HAL_OK;

    /* Set Initialization mode */
    if (BSP_RTC_EnterInitMode(hrtc) != HAL_OK) {
        status = HAL_ERROR;
    } else {
        /* Set RTC COUNTER MSB word */
        WRITE_REG(hrtc->Instance->CNTH, (TimeCounter >> 16U));
        /* Set RTC COUNTER LSB word */
        WRITE_REG(hrtc->Instance->CNTL, (TimeCounter & RTC_CNTL_RTC_CNT));

        /* Wait for synchro */
        if (BSP_RTC_ExitInitMode(hrtc) != HAL_OK) {
            status = HAL_ERROR;
        }
    }

    return status;
}

/**
 * @brief 获取时间和日期
 * @param time 保存时间的结构体
 * @retval SYS_OK/ SYS_ERROR/ SYS_INVALID_PARAM
 */
SYS_StatusTypeDef BSP_RTC_GetTime(datetime_t *time) {
    if (time == NULL) return SYS_INVALID_PARAM;

    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    static uint8_t lastday = 0;
    // 1. 调用 HAL 库读取（注意顺序：必须先读时间，再读日期）
    if (HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != SYS_OK) {
        return SYS_ERROR;
    }
    if (HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != SYS_OK) {
        return SYS_ERROR;
    }

    time->year = 2000 + sDate.Year; // HAL返回的是年份后两位（0-99）
    time->month = sDate.Month;
    time->day = sDate.Date;
    time->hour = sTime.Hours;
    time->minute = sTime.Minutes;
    time->second = sTime.Seconds;
    time->weekday = sDate.WeekDay;

    // 当日期改变时，将时间戳写进BKP寄存器
    if (time->day != lastday) {
        lastday = time->day;
        HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, (uint32_t)((time->day) | (time->month << 8)));
        HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR2, (uint32_t)(time->year));
    }
    return SYS_OK;
}

/**
 * @brief 通过Unix时间戳直接设置RTC秒计数器
 * @param timestamp 从WiFi获取的UTC时间戳
 * @retval SYS_OK/ SYS_ERROR/ SYS_INVALID_PARAM
 */
SYS_StatusTypeDef BSP_RTC_SetTimeUnix(uint32_t timestamp) {
    datetime_t time;
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    timestamp_to_datetime(timestamp, &time);

    // 反向转换：十进制填回 HAL 结构体（注意年份要减去2000）
    sDate.Year = time.year - 2000;
    sDate.Month = time.month;
    sDate.Date = time.day;
    sDate.WeekDay = time.weekday;

    sTime.Hours = time.hour;
    sTime.Minutes = time.minute;
    sTime.Seconds = time.second;

    // 写入硬件（需要解锁后备区域）
    if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != SYS_OK) return SYS_ERROR;
    if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != SYS_OK) return SYS_ERROR;

    return SYS_OK;
}
