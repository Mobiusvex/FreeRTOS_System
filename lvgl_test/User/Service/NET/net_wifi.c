#include "net_wifi.h"
#include "driver_esp8266.h"
#include "stddef.h"
#include "bsp_delay.h"
#include "string.h"
#include "SEGGER_RTT.h"

#define MAX_STEPS 6

typedef struct {
    const char *cmd;
    const char *expect;
    uint8_t max_retry;
    uint32_t timeout_ms;
} step_config_t;

static const step_config_t init_steps[] = {
    {"AT\r\n", "OK", 3, 3000},
    {"AT+RST\r\n", "ready", 3, 5000}, // 注意 RST 回复 "ready"
    {"AT+CWMODE=1\r\n", "OK", 3, 3000},
    {"AT+CIPMUX=0\r\n", "OK", 3, 3000},
    {"AT+CWJAP=\"WifiTest\",\"qwerty789\"\r\n", "WIFI GOT IP", 3, 15000}, // 等待IP
    {"AT+CIPMODE=1\r\n", "OK", 3, 3000}};

static at_cmd_ctx_t step_ctxs[MAX_STEPS];
static uint8_t current_step = 0;
static bool init_done = false;

// 初始化所有上下文
void init_all_contexts(void) {
    for (int i = 0; i < MAX_STEPS; i++) {
        at_cmd_ctx_t *ctx = &step_ctxs[i];
        memset(ctx, 0, sizeof(at_cmd_ctx_t));
        ctx->cmd = init_steps[i].cmd;
        ctx->expect_key = init_steps[i].expect;
        ctx->max_retry = init_steps[i].max_retry;
        ctx->timeout_ticks = init_steps[i].timeout_ms;
        ctx->state = AT_STATE_IDLE;
    }
}

/**
 * @brief 处理单个AT命令
 * @param ctx 上下文
 * @param rx_buf 接收到的缓冲区
 * @param rx_buf_size 缓冲区大小
 * @retval 当前状态
 */
at_state_t net_wifi_at_cmd_process(at_cmd_ctx_t *ctx, uint8_t *rx_buf, uint32_t rx_buf_size) {
    uint8_t *buffer = rx_buf;
    uint32_t rx_len = rx_buf_size;

    switch (ctx->state) {
    case AT_STATE_IDLE:
        // 复位重试计数，进入发送状态
        ctx->retry_cnt = 0;
        ctx->state = AT_STATE_SEND;
        // 注意：不 break，直接进入 SEND 逻辑（或 fall through）
        // 但为了清晰，我们直接进入 SEND 处理
        // 这里 fall through

    case AT_STATE_SEND:
        // 发送命令
        ESP8266_SendCom(ctx->cmd);
        ctx->retry_cnt++;
        ctx->start_tick = BSP_GetTick();
        ctx->reply_handled = false;
        ctx->state = AT_STATE_WAIT_REPLY;
        SEGGER_RTT_printf(0, "AT_CMD_SEND: %s\n", ctx->cmd); // 输出命令
        break;

    case AT_STATE_WAIT_REPLY:
        if (!ctx->reply_handled && rx_len > 0) {
            ctx->reply_handled = true;

            // 检查期望关键词
            bool has_expect = false;
            bool has_error = false;

            if (ctx->expect_key != NULL) {
                if (strstr(buffer, ctx->expect_key) != NULL) {
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
                ctx->state = AT_STATE_SUCCESS;
                SEGGER_RTT_printf(0, "AT_CMD_SUCCESS: %s\n", ctx->cmd);
            } else if (has_error) {
                ctx->state = AT_STATE_FAIL;
            } else {
                // 收到了其他无关行，忽略，继续等待（重置回复处理标志，允许下一条）
                ctx->reply_handled = false;
            }
        }
        // 检查超时（无论是否收到数据，超时都要处理）
        if (ctx->state == AT_STATE_WAIT_REPLY) {
            uint32_t now = BSP_GetTick();
            if ((now - ctx->start_tick) >= ctx->timeout_ticks) {
                // 超时，判断是否重试
                if (ctx->retry_cnt < ctx->max_retry) {
                    ctx->state = AT_STATE_SEND; // 重发
                } else {
                    ctx->state = AT_STATE_TIMEOUT; // 最终超时
                }
            }
        }
        break;

    // 终止状态，不做任何操作
    case AT_STATE_SUCCESS:
    case AT_STATE_FAIL:
    case AT_STATE_TIMEOUT:
    case AT_STATE_DONE:
    default:
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
at_state_t net_wifi_mode_set(uint8_t *rx_buf, uint32_t rx_buf_size) {
    if (current_step < MAX_STEPS) { // 确保没有越界
        at_cmd_ctx_t *ctx = &step_ctxs[current_step];
        at_state_t state = net_wifi_at_cmd_process(ctx, rx_buf, rx_buf_size);
        if (state == AT_STATE_SUCCESS) {
            current_step++;
            if (current_step == MAX_STEPS) {
                init_done = true;
            }
        }
    }
    return init_done ? AT_STATE_DONE : AT_STATE_IDLE;
}

/**
 * @brief 清空状态，准备开始连接
 */
void net_wifi_connect(void) {
    if (!init_done) {
        init_all_contexts();
    }
    current_step = 0; // 重置步骤计数器
}