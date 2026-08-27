#include "net_time.h"
#include "net_wifi.h"
#include "stddef.h"
#include "stdio.h"
#include <stdlib.h>
#include <string.h>
#include "SEGGER_RTT.h"

typedef enum {
    NET_TIME_CIP_MODE1_SET,
    NET_TIME_TCP_CONNECT,
    NET_TIME_CIP_SEND,
    NET_TIME_GET_TIME,
    NET_TIME_EXIT_CIP_MODE1,
    NET_TIME_CIP_CLOSE,
    NET_TIME_STEP_NUM
} net_time_step_t;

enum {
    TIME_NAME = 0,
    TIME_CODE_NUM
};

const char *net_time_fields[TIME_CODE_NUM] = {
    "server_time",
};
at_cmd_ctx_t net_time_step_ctx[NET_TIME_STEP_NUM];
static uint32_t net_timestamp = 0;

static cmd_state_t parseTimeJSON(char *jsonData, uint64_t *timestamp);
static cmd_state_t receive_time_analysis(at_cmd_ctx_t *ctx, uint8_t *buffer, uint32_t rx_len);

static const step_config_t net_time_step[] = {
    {"AT+CIPSTART=\"TCP\",\"api.pinduoduo.com\",80\r\n", "OK", 3, 3, 3000},
    {"AT+CIPMODE=1\r\n", "OK", 3, 3, 3000},
    {"AT+CIPSEND\r\n", "OK", 3, 3, 3000},
    {"GET http://api.pinduoduo.com/api/server/_stm\r\n", "", 3, 3, 3000},
    {"+++", "+++", 3, 3, 3000},
    {"AT+CIPCLOSE\r\n", "OK", 3, 3, 3000}};

/**
 * @brief 初始化时间上下文
 * @retval 无
 */
static void net_time_init_ctx(void) {
    for (int i = 0; i < NET_TIME_STEP_NUM; i++) {
        at_cmd_ctx_t *ctx = &net_time_step_ctx[i];
        memset(ctx, 0, sizeof(at_cmd_ctx_t));
        ctx->cmd = net_time_step[i].cmd;
        ctx->expect_key = net_time_step[i].expect;
        ctx->timeout_max_retry = net_time_step[i].timeout_max_retry;
        ctx->fail_max_retry = net_time_step[i].fail_max_retry;
        ctx->timeout_ticks = net_time_step[i].timeout_ms;
        ctx->is_care_for_error = true;
        ctx->state = CMD_STATE_IDLE;
        if (i == NET_TIME_GET_TIME) {
            ctx->receive_cb = receive_time_analysis;
        } else {
            ctx->receive_cb = receive_standard_analysis;
        }
        if (i == NET_TIME_CIP_MODE1_SET || i == NET_TIME_CIP_SEND || i == NET_TIME_TCP_CONNECT) {
            ctx->is_care_for_error = false;
        }
    }
}

/**
 * @brief 时间戳json解析
 * @param ctx 上下文
 * @param buffer 接收到的缓冲区
 * @param rx_len 接收到的缓冲区大小
 * @retval 当前状态
 */
static cmd_state_t receive_time_analysis(at_cmd_ctx_t *ctx, uint8_t *buffer, uint32_t rx_len) {
    // 检查期望关键词
    cmd_state_t ret = CMD_STATE_WAIT_REPLY;
    bool has_expect = false;
    bool has_error = false;
    uint64_t timestamp = 0;
    // 检测常见错误
    if (strstr(buffer, "invalid") || strstr(buffer, "fail")) {
        ret = CMD_STATE_FAIL;
    } else {
        ret = parseTimeJSON(buffer, &timestamp);
    }
    if (ret == CMD_STATE_SUCCESS) {
        net_timestamp = timestamp / 1000; // 从微秒转换为秒
        SEGGER_RTT_printf(0, "timestamp: %d\n", net_timestamp);
    }
    return ret;
}

/**
 * @brief 按步骤获取时间戳
 * @param ctx 上下文
 * @param rx_buf 接收到的缓冲区
 * @param rx_len 接收到的缓冲区大小
 * @retval 当前状态
 */
cmd_state_t net_time_mode_set(uint8_t *rx_buf, uint32_t rx_buf_size) {
    static cmd_state_t state = CMD_STATE_IDLE;
    static net_time_step_t net_time_current_step = NET_TIME_CIP_MODE1_SET;

    if (net_time_current_step < NET_TIME_STEP_NUM) { // 确保没有越界
        at_cmd_ctx_t *ctx = &net_time_step_ctx[net_time_current_step];
        state = cmd_send_and_judge_process(ctx, rx_buf, rx_buf_size);
        if (state == CMD_STATE_SUCCESS || (!ctx->is_care_for_error && state == CMD_STATE_FAIL)) {
            net_time_current_step++;
            if (net_time_current_step == NET_TIME_STEP_NUM) {
                net_time_current_step = NET_TIME_CIP_MODE1_SET;
                return CMD_STATE_DONE;
            }
            return CMD_STATE_SUCCESS; // 本次流程完成
        } else if (state == CMD_STATE_TIMEOUT || state == CMD_STATE_FAIL) {
            net_time_current_step = NET_TIME_CIP_MODE1_SET;
        }
    }
    return state;
}

/**
 * @brief 清空状态，准备开始连接
 */
void net_time_init(void) {
    net_time_init_ctx();
}

/**
 * @brief 解析时间JSON数据
 * @param jsonData  JSON数据
 * @param timestamp  解析后的时间数据
 */
static cmd_state_t parseTimeJSON(char *jsonData, uint64_t *timestamp) {
    // TODO: 需要保护，防止数据撕裂,或者完成后通过列队发送给显示屏
    static const char cut_str[] = "{},[]/ +\":";
    cmd_state_t ret = CMD_STATE_WAIT_REPLY;
    char *endptr;
    uint8_t index = 0;
    // NOTE: strok会改变原字符串，但原字符串每次用完就扔，所以这里不拷贝一份
    char *token = strtok(jsonData, cut_str);
    while (token != NULL) {
        if (strcmp(token, net_time_fields[TIME_NAME]) == 0) {
            token = strtok(NULL, cut_str);
            *timestamp = strtoull(token, &endptr, 10);
            if (endptr != token && *endptr == '\0') {
                return CMD_STATE_SUCCESS; // 解析成功
            }
        }
        token = strtok(NULL, cut_str);
    }
    return ret;
}

/**
 * @brief 获取当前时间戳
 * @retval 当前时间戳（秒）
 */
uint32_t net_time_get_time(void) {
    return net_timestamp;
}