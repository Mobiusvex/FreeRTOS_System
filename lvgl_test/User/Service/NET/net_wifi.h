#ifndef NET_WIFI_H
#define NET_WIFI_H
#include "stdint.h"
#include "stdbool.h"

typedef enum {
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_CONNECTED
} wifi_state_t;

// 状态定义
typedef enum {
    CMD_STATE_IDLE = 0,   // 空闲，准备开始
    CMD_STATE_SEND,       // 发送命令（或重发）
    CMD_STATE_WAIT_REPLY, // 等待回复
    CMD_STATE_REPLY_CMD,  // 成功收到命令响应
    CMD_STATE_SUCCESS,    // 成功收到期望回复
    CMD_STATE_FAIL,       // 收到错误回复（ERROR/FAIL）
    CMD_STATE_TIMEOUT,    // 超时且重试次数用尽
    CMD_STATE_DONE        // 整体流程完成（用于多步骤）
} cmd_state_t;

typedef struct {
    const char *cmd;
    const char *expect;
    uint8_t timeout_max_retry;
    uint8_t fail_max_retry;
    uint32_t timeout_ms;
} step_config_t;

// 上下文结构体（由调用者维护）
typedef struct at_cmd_ctx {
    // ---- 状态控制 ----
    cmd_state_t state;         // 当前状态
    uint8_t timeout_retry_cnt; // 当前超时已重试次数
    uint8_t timeout_max_retry; // 最大允许超时重试次数

    uint8_t fail_retry_cnt; // 当前错误已重试次数
    uint8_t fail_max_retry; // 最大允许出错重试次数

    // ---- 命令与期望 ----
    const char *cmd;        // 要发送的AT命令（需包含 \r\n）
    const char *expect_key; // 期望在回复中匹配的关键词（如 "OK"、"WIFI GOT IP"）
                            // 若为 NULL，则只检测没有 ERROR/FAIL 就算成功
    uint16_t tx_timeout;    // 发送命令的超时时间（ms）

    // ---- 超时控制 ----
    uint32_t start_tick;    // 开始等待时刻（xTaskGetTickCount()）
    uint32_t timeout_ticks; // 等待超时时间（pdMS_TO_TICKS(ms)）

    // ---- 内部标志 ----
    bool reply_handled;                                                  // 防止重复处理同一行
    bool is_care_for_error;                                              // 是否关心 ERROR/FAIL
    cmd_state_t (*receive_cb)(struct at_cmd_ctx *, uint8_t *, uint32_t); // 接收到回复后的回调函数
} at_cmd_ctx_t;

cmd_state_t cmd_send_and_judge_process(at_cmd_ctx_t *ctx, uint8_t *rx_buf, uint32_t rx_buf_size);
cmd_state_t net_wifi_mode_set(uint8_t *rx_buf, uint32_t rx_buf_size);
cmd_state_t receive_standard_analysis(at_cmd_ctx_t *ctx, uint8_t *buffer, uint32_t rx_len);
void net_wifi_init(void);

#endif // NET_WIFI_H