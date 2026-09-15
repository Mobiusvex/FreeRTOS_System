// bsp_rtc.h
#ifndef __BSP_RTC_H
#define __BSP_RTC_H

#include "sys_defs.h"
#include <stdint.h>
#include "time_convert.h" // 自定义时间转换头文件
#include "stm32f1xx_hal.h"

SYS_StatusTypeDef BSP_RTC_GetTime(datetime_t *time);
SYS_StatusTypeDef BSP_RTC_SetTimeUnix(uint32_t timestamp);
uint32_t BSP_RTC_ReadTimeCounter(RTC_HandleTypeDef *hrtc);
#endif