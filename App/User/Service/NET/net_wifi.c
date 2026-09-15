#include "net_wifi.h"
#include "driver_esp8266.h"
#include "stddef.h"
#include "bsp_delay.h"
#include "string.h"
#include "debug_func.h"
#include "app_config.h"

typedef enum {
    START_AT,
    AT_RST,
    AT_CWMODE1,
    AT_CIPMUX0,
    AT_CWJAP,
    WIFI_STEP_NUM
} net_wifi_step_t;

static const step_config_t net_wifi_step[] = {
    {"AT\r\n", "OK", 3, 3, 3000},
    {"AT+RST\r\n", "OK", 3, 3, 5000}, // 注意 RST 回复 "ready"
    {"AT+CWMODE=1\r\n", "OK", 3, 3, 3000},
    {"AT+CIPMUX=0\r\n", "OK", 3, 3, 3000},
    {"AT+CWJAP=\"" WIFI_SSID "\",\"" WIFI_PASSWORD "\"\r\n", "OK", 3, 3, 15000} // 等待IP
};

static at_cmd_ctx_t net_wifi_step_ctxs[WIFI_STEP_NUM];

cmd_state_t receive_standard_analysis(at_cmd_ctx_t *ctx, uint8_t *buffer, uint32_t rx_len);

/**
 * @brief 初始化AT命令上下文
 * @retval 无
 */
static void net_wifi_init_ctx(void) {
    for (int i = 0; i < WIFI_STEP_NUM; i++) {
        at_cmd_ctx_t *ctx = &net_wifi_step_ctxs[i];
        memset(ctx, 0, sizeof(at_cmd_ctx_t));
        ctx->cmd = net_wifi_step[i].cmd;
        ctx->expect_key = net_wifi_step[i].expect;
        ctx->timeout_max_retry = net_wifi_step[i].timeout_max_retry;
        ctx->fail_max_retry = net_wifi_step[i].fail_max_retry;
        ctx->is_care_for_error = true;
        ctx->timeout_ticks = net_wifi_step[i].timeout_ms;
        ctx->state = CMD_STATE_IDLE;
        ctx->receive_cb = receive_standard_analysis;
        ctx->tx_timeout = 10;
    }
}

/**
 * @brief 标准分析函数，用于处理接收到的AT命令回复
 * @param ctx 上下文
 * @param buffer 接收到的缓冲区
 * @param rx_len 接收到的缓冲区大小
 * @retval 当前状态
 */
cmd_state_t receive_standard_analysis(at_cmd_ctx_t *ctx, uint8_t *buffer, uint32_t rx_len) {
    // 检查期望关键词
    cmd_state_t ret = ctx->state;
    bool has_expect = false;
    bool has_error = false;

    if (ctx->expect_key != NULL) {
        if (strstr(buffer, ctx->cmd) != NULL) {
            ret = CMD_STATE_REPLY_CMD;
        }
        // 回复的CMD和OK分两次或一起接收
        if ((ret == CMD_STATE_REPLY_CMD)
            && strstr(buffer, ctx->expect_key) != NULL) {
            has_expect = true;
        }
    } else {
        // 如果没有期望关键词，则只要没有 ERROR/FAIL 就算成功
        has_expect = true; // 默认视为成功，但需排除错误
    }

    // 检测常见错误
    if (strstr(buffer, "ERROR") || strstr(buffer, "FAIL")) {
        has_error = true;
    }

    if (has_expect && !has_error) {
        ret = CMD_STATE_SUCCESS;
        RTT_PRINTF("AT_CMD_SUCCESS: %s\n", ctx->cmd);
    } else if (has_error) {
        ret = CMD_STATE_FAIL;
    } else {
        // 收到了其他无关行，忽略，继续等待（重置回复处理标志，允许下一条）
    }
    return ret;
}

/**
 * @brief 处理单个AT命令
 * @param ctx 上下文
 * @param rx_buf 接收到的缓冲区
 * @param rx_buf_size 缓冲区大小
 * @retval 当前状态
 */
cmd_state_t cmd_send_and_judge_process(at_cmd_ctx_t *ctx, uint8_t *rx_buf, uint32_t rx_buf_size) {
    uint8_t *buffer = rx_buf;
    uint32_t rx_len = rx_buf_size;

    switch (ctx->state) {
    case CMD_STATE_IDLE:
        // 复位重试计数，进入发送状态
        ctx->timeout_retry_cnt = 0;
        ctx->fail_retry_cnt = 0;
        ctx->state = CMD_STATE_SEND;
        // 注意：不 break，直接进入 SEND 逻辑（或 fall through）
        // 但为了清晰，我们直接进入 SEND 处理
        // 这里 fall through

    case CMD_STATE_SEND:
        // 发送命令
        ESP8266_SendCom(ctx->cmd, ctx->tx_timeout);
        ctx->timeout_retry_cnt++;
        ctx->fail_retry_cnt++;
        ctx->start_tick = BSP_GetTick();
        ctx->reply_handled = false;
        ctx->state = CMD_STATE_WAIT_REPLY;
        RTT_PRINTF("AT_CMD_SEND: %s\n", ctx->cmd); // 输出命令
        break;
    case CMD_STATE_WAIT_REPLY:
    case CMD_STATE_REPLY_CMD:

        if (!ctx->reply_handled && rx_len > 0) {
            ctx->reply_handled = true;
            ctx->state = ctx->receive_cb(ctx, buffer, rx_len);
            // 没收到需要的回复或只收了部分数据，继续等待
            if (ctx->state == CMD_STATE_WAIT_REPLY || ctx->state == CMD_STATE_REPLY_CMD) {
                ctx->reply_handled = false;
            }
        }
        if (ctx->state == CMD_STATE_FAIL) {
            if (ctx->fail_retry_cnt < ctx->fail_max_retry) {
                ctx->state = CMD_STATE_WAIT_REPLY; // 失败也等满3S再重发，不然ESP8266容易卡死
            } else {
                ctx->state = CMD_STATE_FAIL; // 最终失败
            }
        }
        // 检查超时（无论是否收到数据，超时都要处理）
        if (ctx->state == CMD_STATE_WAIT_REPLY || ctx->state == CMD_STATE_REPLY_CMD) {
            uint32_t now = BSP_GetTick();
            if ((now - ctx->start_tick) >= ctx->timeout_ticks) {
                // 超时，判断是否重试
                if (ctx->timeout_retry_cnt < ctx->timeout_max_retry) {
                    ctx->state = CMD_STATE_SEND; // 重发
                } else {
                    ctx->state = CMD_STATE_TIMEOUT; // 最终超时
                }
            }
        }
        break;

    // 终止状态，不做任何操作
    case CMD_STATE_SUCCESS:
    case CMD_STATE_FAIL:
    case CMD_STATE_TIMEOUT:
    case CMD_STATE_DONE:
    default:
        ctx->state = CMD_STATE_IDLE; // 重置状态
        break;
    }
    return ctx->state;
}
/**
 * @brief 处理所有AT命令
 * @param rx_buf 接收到的缓冲区
 * @param rx_buf_size 缓冲区大小
 * @retval 当前状态
 */
cmd_state_t net_wifi_mode_set(uint8_t *rx_buf, uint32_t rx_buf_size) {
    cmd_state_t state = CMD_STATE_IDLE;
    static uint8_t net_wifi_current_step = 0;
    if (net_wifi_current_step < WIFI_STEP_NUM) { // 确保没有越界
        at_cmd_ctx_t *ctx = &net_wifi_step_ctxs[net_wifi_current_step];
        state = cmd_send_and_judge_process(ctx, rx_buf, rx_buf_size);
        if (state == CMD_STATE_SUCCESS || (!ctx->is_care_for_error && state == CMD_STATE_FAIL)) {
            net_wifi_current_step++;
            if (net_wifi_current_step == WIFI_STEP_NUM) {
                net_wifi_current_step = 0;
                state = CMD_STATE_DONE; // 所有命令都成功
            } else {
                state = CMD_STATE_SUCCESS;
            }
        } else if (state == CMD_STATE_TIMEOUT || state == CMD_STATE_FAIL) {
            net_wifi_current_step = 0;
        }
    }
    return state;
}

/**
 * @brief 清空状态，准备开始连接
 */
void net_wifi_init(void) {
    net_wifi_init_ctx();
}