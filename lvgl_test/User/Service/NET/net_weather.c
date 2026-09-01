#include "net_weather.h"
#include "net_wifi.h"
#include "stddef.h"
#include "stdio.h"
#include <stdlib.h>
#include <string.h>
#include "SEGGER_RTT.h"
#include "cmsis_os2.h"

#define WEATHER_GET_CMD_LENGTH 256
enum {
    WEATHER_CITY_NAME,
    WEATHER_COUNTRY,
    WEATHER_TEXT,
    WEATHER_TEMPERATURE,
    WEATHER_CODE_NUM
};

typedef enum {
    NET_WEATHER_CIP_MODE1_SET,
    NET_WEATHER_TCP_CONNECT,
    NET_WEATHER_CIP_SEND,
    NET_WEATHER_GET_WEATHER,
    NET_WEATHER_EXIT_CIP_MODE1,
    NET_WEATHER_CIP_CLOSE,
    NET_WEATHER_STEP_NUM
} net_weather_step_t;

// 列出需要解析的字段
typedef struct {
    char city[25];
    char country[10];
    char weather[10];
    int8_t temperature;
} weather_data_t;

char weather_get_cmd_buf[WEATHER_GET_CMD_LENGTH];

volatile char send_city[25] = "enshi"; // 城市名称

weather_data_t weather_data = {"enshi", "China", "Sunny", 25};

const char *net_weather_fields[WEATHER_CODE_NUM] = {
    "name",
    "country",
    "text",
    "temperature",
};

static const step_config_t net_weather_step[] = {
    {"AT+CIPSTART=\"TCP\",\"api.seniverse.com\",80\r\n", "OK", 3, 3, 3000},
    {"AT+CIPMODE=1\r\n", "OK", 3, 3, 3000},
    {"AT+CIPSEND\r\n", "OK", 3, 3, 3000},
    {"--", "", 3, 3, 3000},
    {"+++", "+++", 3, 3, 3000},
    {"AT+CIPCLOSE\r\n", "OK", 3, 3, 3000}};

at_cmd_ctx_t net_weather_step_ctx[NET_WEATHER_STEP_NUM];

static uint16_t build_weather_request(char *buffer, uint16_t buf_size, const char *city);
static cmd_state_t parseWeatherJSON(char *jsonData, weather_data_t *p_weatherData);
static cmd_state_t receive_weather_analysis(at_cmd_ctx_t *ctx, uint8_t *buffer, uint32_t rx_len);

/**
 * @brief 初始化天气上下文
 * @retval 无
 */
static void net_weather_init_ctx(void) {
    for (int i = 0; i < NET_WEATHER_STEP_NUM; i++) {
        at_cmd_ctx_t *ctx = &net_weather_step_ctx[i];
        memset(ctx, 0, sizeof(at_cmd_ctx_t));
        ctx->cmd = net_weather_step[i].cmd;
        ctx->expect_key = net_weather_step[i].expect;
        ctx->timeout_max_retry = net_weather_step[i].timeout_max_retry;
        ctx->fail_max_retry = net_weather_step[i].fail_max_retry;
        ctx->timeout_ticks = net_weather_step[i].timeout_ms;
        ctx->is_care_for_error = true;
        ctx->state = CMD_STATE_IDLE;
        ctx->tx_timeout = 10;
        if (i == NET_WEATHER_GET_WEATHER) {
            build_weather_request(weather_get_cmd_buf, WEATHER_GET_CMD_LENGTH, send_city);
            ctx->cmd = weather_get_cmd_buf;
            ctx->receive_cb = receive_weather_analysis;
        } else {
            ctx->receive_cb = receive_standard_analysis;
        }
        if (i == NET_WEATHER_CIP_MODE1_SET || i == NET_WEATHER_CIP_SEND || i == NET_WEATHER_TCP_CONNECT) {
            ctx->is_care_for_error = false;
        }
    }
}

/**
 * @brief 天气json解析
 * @param ctx 上下文
 * @param buffer 接收到的缓冲区
 * @param rx_len 接收到的缓冲区大小
 * @retval 当前状态
 */
static cmd_state_t receive_weather_analysis(at_cmd_ctx_t *ctx, uint8_t *buffer, uint32_t rx_len) {
    // 检查期望关键词
    cmd_state_t ret = CMD_STATE_WAIT_REPLY;
    bool has_expect = false;
    bool has_error = false;
    // 检测常见错误
    if (strstr(buffer, "invalid") || strstr(buffer, "fail")) {
        ret = CMD_STATE_FAIL;
    } else {
        ret = parseWeatherJSON(buffer, &weather_data);
    }
    SEGGER_RTT_printf(0, "city:%s,country:%s,weather:%s,temperature:%d\r\n",
                      weather_data.city, weather_data.country, weather_data.weather, weather_data.temperature);
    return ret;
}

/**
 * @brief 按步骤获取天气
 * @param ctx 上下文
 * @param rx_buf 接收到的缓冲区
 * @param rx_len 接收到的缓冲区大小
 * @retval 当前状态
 */
cmd_state_t net_weather_mode_set(uint8_t *rx_buf, uint32_t rx_buf_size) {
    static cmd_state_t state = CMD_STATE_IDLE;
    static net_weather_step_t net_weather_current_step = NET_WEATHER_CIP_MODE1_SET;

    if (net_weather_current_step < NET_WEATHER_STEP_NUM) { // 确保没有越界
        at_cmd_ctx_t *ctx = &net_weather_step_ctx[net_weather_current_step];
        state = cmd_send_and_judge_process(ctx, rx_buf, rx_buf_size);
        if (state == CMD_STATE_SUCCESS || (!ctx->is_care_for_error && state == CMD_STATE_FAIL)) {
            net_weather_current_step++;
            if (net_weather_current_step == NET_WEATHER_STEP_NUM) {
                net_weather_current_step = NET_WEATHER_CIP_MODE1_SET;
                return CMD_STATE_DONE;
            }
            return CMD_STATE_SUCCESS; // 本次流程完成
        } else if (state == CMD_STATE_TIMEOUT || state == CMD_STATE_FAIL) {
            net_weather_current_step = NET_WEATHER_CIP_MODE1_SET;
        }
    }
    return state;
}

/**
 * @brief 清空状态，准备开始连接
 */
void net_weather_init(void) {
    net_weather_init_ctx();
}

/**
 * @brief 构建心知天气 API 请求字符串
 * @param buffer     输出缓冲区
 * @param buf_size   缓冲区大小
 * @param city       城市名称
 * @retval 成功返回写入的字符数（不含终止符），失败返回负数
 */
static uint16_t build_weather_request(char *buffer, uint16_t buf_size, const char *city) {
    // 直接格式化：将 %s 替换为传入的城市名
    return snprintf(buffer, buf_size,
                    "GET https://api.seniverse.com/v3/weather/now.json"
                    "?key=SwLQ3i0Q5TNa6NSKT&location=%s&language=en&unit=c\r\n",
                    city);
}

/**
 * @brief 解析天气JSON数据
 * @param jsonData  JSON数据
 * @param p_weatherData  解析后的天气数据
 */
static cmd_state_t parseWeatherJSON(char *jsonData, weather_data_t *p_weatherData) {
    // TODO: 需要保护，防止数据撕裂,或者完成后通过列队发送给显示屏
    static const char cut_str[] = "{},[]/ +\":";
    cmd_state_t ret = CMD_STATE_WAIT_REPLY;
    uint8_t index = 0;
    // NOTE: strok会改变原字符串，但原字符串每次用完就扔，所以这里不拷贝一份
    char *token = strtok(jsonData, cut_str);
    while (token != NULL) {
        if (strcmp(token, net_weather_fields[WEATHER_CITY_NAME]) == 0) {
            token = strtok(NULL, cut_str);
            strcpy(p_weatherData->city, token);
            index++;
        } else if (strcmp(token, net_weather_fields[WEATHER_COUNTRY]) == 0) {
            token = strtok(NULL, cut_str);
            if (strstr(token, "CN")) {
                strcpy(weather_data.country, "China");
            } else {
                strcpy(weather_data.country, token);
            }
            index++;
        } else if (strcmp(token, net_weather_fields[WEATHER_TEXT]) == 0) {
            token = strtok(NULL, cut_str);
            strcpy(weather_data.weather, token);
            index++;
        } else if (strcmp(token, net_weather_fields[WEATHER_TEMPERATURE]) == 0) {
            token = strtok(NULL, cut_str);
            weather_data.temperature = atoi(token);
            index++;
        }
        token = strtok(NULL, cut_str);
    }
    if (index == WEATHER_CODE_NUM) {
        ret = CMD_STATE_SUCCESS;
    }
    return ret;
}

/**
 * @brief 设置城市名称
 * @param city 城市名称
 * @retval 无
 * /
 */
void set_weather_city(const char *city) {
    uint32_t lock_state = osKernelLock();
    strcpy(send_city, city);
    build_weather_request(weather_get_cmd_buf, WEATHER_GET_CMD_LENGTH, send_city);
    osKernelRestoreLock(lock_state);
}
