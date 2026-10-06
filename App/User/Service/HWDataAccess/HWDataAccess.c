#include "HWDataAccess.h"

#include "driver_dht11.h"
#include "driver_mpu6050.h"
#include "driver_esp8266.h"
#include "driver_vs1053b.h"
#include "cmd_audio.h"
#include <string.h>
#include "bsp_rtc.h"
#include "debug_func.h"
#include "stdio.h"

extern osMessageQueueId_t xAudioCmdQueue;
extern osMessageQueueId_t xCmdDisplayQueue;
/**
 * @brief DHT11传感器初始化
 * @retval 初始化成功返回SYS_OK，初始化失败返回SYS_ERROR
 */
SYS_StatusTypeDef HW_DHT11_Init(void) {
    // Initialize DHT11 sensor
    DHT11_Init();
    return SYS_OK;
}
/**
 * @brief 读取DHT11传感器的湿度和温度
 * @param humi 指向存储湿度值的指针
 * @param temp 指向存储温度值的指针
 * @retval 读取成功返回SYS_OK，读取失败返回SYS_ERROR
 */
SYS_StatusTypeDef HW_DHT11_Get_Humi_Temp(int16_t *humi, int16_t *temp) {
    return DHT11_Read(humi, temp);
}

/**
 * @brief MPU6050传感器初始化
 * @param 无
 * @retval 初始化成功返回SYS_OK
 */
SYS_StatusTypeDef HW_MPU6050_Init(void) {
    // Initialize MPU6050 sensor
    return MPU6050_Init();
}

/**
 * @brief 读取MPU6050传感器的角度
 * @param pitch 指向存储俯仰角的指针
 * @param roll 指向存储滚转角的指针
 * @param yaw 指向存储偏航角的指针
 * @retval 读取成功返回SYS_OK
 */
SYS_StatusTypeDef HW_MPU6050_Get_Angle(float *pitch, float *roll, float *yaw) {
    return MPU6050_getAngle(pitch, roll, yaw);
}

/**
 * @brief 将角度值转换为整数 放大十倍，四舍五入
 * @param pitch 俯仰角
 * @param roll 滚转角
 * @param yaw 偏航角
 * @param pitch_int 指向存储整数俯仰角的指针
 * @param roll_int 指向存储整数滚转角的指针
 * @param yaw_int 指向存储整数偏航角的指针
 */
void HW_MPU6050_AngleFloatToInt(float pitch, float roll, float yaw, int16_t *pitch_int, int16_t *roll_int, int16_t *yaw_int) {
    *pitch_int = (int16_t)(pitch * 10.0f + (pitch >= 0 ? 0.5f : -0.5f));
    *roll_int = (int16_t)(roll * 10.0f + (roll >= 0 ? 0.5f : -0.5f));
    *yaw_int = (int16_t)(yaw * 10.0f + (yaw >= 0 ? 0.5f : -0.5f));
}
/**
 * @brief ESP8266传感器初始化
 * @param 无
 * @retval 初始化成功返回SYS_OK
 */
SYS_StatusTypeDef HW_ESP8266_Init(void) {
    return ESP8266_Init(); // Initialize ESP8266 sensor
}

SYS_StatusTypeDef HW_ESP8266_Reset(void) {
    return ESP8266_Reset(); // Reset ESP8266 sensor
}

/**
 * @brief 获取当前时间字符串
 * @param buffer 指向存储时间字符串的缓冲区
 * @param len 缓冲区长度
 * @retval 获取成功返回SYS_OK，获取失败返回SYS_ERROR
 */
SYS_StatusTypeDef HW_Time_GetString(char *buffer, uint8_t len) {
    datetime_t now;
    if (BSP_RTC_GetTime(&now) == SYS_OK) {
        snprintf(buffer, len, "%04d-%02d-%02d %02d:%02d:%02d",
                 now.year, now.month, now.day, now.hour, now.minute, now.second);
        return SYS_OK;
    } else {
        return SYS_ERROR; // 获取RTC时间失败1787730595597
    }
}

SYS_StatusTypeDef HW_Time_Getdata(datetime_t *data) {
    if (BSP_RTC_GetTime(data) == SYS_OK) {
        return SYS_OK;
    } else {
        return SYS_ERROR; // 获取RTC时间失败1787730595597
    }
}

#define TIMESTAMP_TIME_2000_OFFSET 946656000

/**
 * @brief 设置RTC时间
 * @param timestamp_sec 时间戳（秒）
 * @retval 设置成功返回SYS_OK，设置失败返回SYS_ERROR
 */
SYS_StatusTypeDef HW_Time_SetRTC(uint32_t timestamp_sec) {
    return BSP_RTC_SetTimeUnix(timestamp_sec - TIMESTAMP_TIME_2000_OFFSET);
}

void HW_LED_SetState(LED_NAME_T led_name, LED_STATE_T led_state) {
    led_set_state(led_name, led_state);
}

void HW_LED_GetState(LED_NAME_T led_name, LED_STATE_T *led_state) {
    led_get_state(led_name, led_state);
}

SYS_StatusTypeDef HW_AUDIO_Init(void) {
    if (!VS1053_Init()) {
        return SYS_ERROR;
    }
    VS1053_StopPlay();
    return SYS_OK;
}

void HW_AUDIO_Reset(void) {
    VS1053_Reset();
    VS1053_StopPlay();
}

enum_slot_t HW_AUDIO_Play(enum_slot_t slot) {
    char filename[AUDIO_META_NAME_LEN];
    uint32_t file_size;
    static uint8_t buf[512];
    SYS_DataEventType_t event;
    uint8_t volume = 0xFE, new_volume = 0xFE;
    uint8_t last_progress = 0, new_progress = 0;
    if (!AUDIO_GetInfo(slot, filename, &file_size)) {
        RTT_PRINTF("Voice: slot %u empty\n", slot);
        return AUDIO_SLOT_NONE;
    }
    RTT_PRINTF("Playing '%s' (%u bytes)\n", filename, file_size);

    for (uint32_t off = 0; off < file_size;) {
        uint8_t next;
        if (osMessageQueueGet(xAudioCmdQueue, &next, NULL, 0) == osOK) /* 被打断 */
            return next;
        uint32_t chunk = file_size - off;
        if (chunk > sizeof(buf)) chunk = sizeof(buf);

        SYS_DATA_GetVolume(&new_volume);
        if (new_volume != volume) {
            VS1053_SetVolumePercent(new_volume);
            volume = new_volume; // 更新音量
        }
        if (!AUDIO_ReadData(slot, off, buf, (uint16_t)chunk) || !VS1053_WriteSdiBlocking(buf, (uint16_t)chunk)) {
            RTT_PRINTF("play slot %u fail @%u\n", slot, off);
            break;
        }
        off += chunk;
        new_progress = off * 100 / file_size;
        if (new_progress != last_progress) { // 更新进度
            SYS_DATA_SetAudioPlayerProgress(new_progress);
            last_progress = new_progress;
            event = SYS_MUSIC_PLAYER_PROGRESS_UPDATE;
            osMessageQueuePut(xCmdDisplayQueue, &event, 0, 0);
        }
    }
    return AUDIO_SLOT_NONE;
}

void HW_AUDIO_Stop(void) {
    VS1053_StopPlay();
}

void HW_AUDIO_PUT(enum_slot_t slot) {
    if (xAudioCmdQueue == NULL) return;

    osMessageQueueReset(xAudioCmdQueue); /* 丢掉旧请求 */
    osMessageQueuePut(xAudioCmdQueue, &slot, 0, 0);
}
HW_InterfaceTypeDef HW_Interface = {
    .DHT11 = {
        .ConnectionError = 1,
        .update_time = 800,
        .data_status = SYS_ERROR,
        .Init = HW_DHT11_Init,
        .GetHumiTemp = HW_DHT11_Get_Humi_Temp},
    .MPU6050 = {// MPU6050 sensor
                .ConnectionError = 1,
                .update_time = 900,
                .data_status = SYS_ERROR,
                .Init = HW_MPU6050_Init,
                .GetAngle = HW_MPU6050_Get_Angle,
                .AngleFloatToInt = HW_MPU6050_AngleFloatToInt},
    .ESP8266 = {// ESP8266 sensor
                .ConnectionError = 1,
                .time_fetch_interval = 3600000,
                .onenet_report_interval = 30000,
                .data_status = SYS_ERROR,
                .Init = HW_ESP8266_Init,
                .Reset = HW_ESP8266_Reset},
    .RealTimeClock = {//
                      .update_time = 1000,
                      .GetTimeString = HW_Time_GetString,
                      .SetTimedata = HW_Time_Getdata,
                      .SetTimestamp = HW_Time_SetRTC},
    .LED = {//
            .SetLEDState = HW_LED_SetState,
            .GetLEDState = HW_LED_GetState},
    .AUDIO = {//
              .ConnectionError = 1,
              .Init = HW_AUDIO_Init,
              .Reset = HW_AUDIO_Reset,
              .PlayVoice = HW_AUDIO_Play,
              .StopVoice = HW_AUDIO_Stop,
              .PlayVoicePut = HW_AUDIO_PUT},
};
