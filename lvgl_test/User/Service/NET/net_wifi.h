#ifndef NET_WIFI_H
#define NET_WIFI_H
#include "stdint.h"
#include "stdbool.h"

// 状态定义
typedef enum {
    AT_STATE_IDLE = 0,   // 空闲，准备开始
    AT_STATE_SEND,       // 发送命令（或重发）
    AT_STATE_WAIT_REPLY, // 等待回复
    AT_STATE_SUCCESS,    // 成功收到期望回复
    AT_STATE_FAIL,       // 收到错误回复（ERROR/FAIL）
    AT_STATE_TIMEOUT,    // 超时且重试次数用尽
    AT_STATE_DONE        // 整体流程完成（用于多步骤）
} at_state_t;

// 上下文结构体（由调用者维护）
typedef struct {
    // ---- 状态控制 ----
    at_state_t state;  // 当前状态
    uint8_t retry_cnt; // 当前已重试次数
    uint8_t max_retry; // 最大允许重试次数

    // ---- 命令与期望 ----
    const char *cmd;        // 要发送的AT命令（需包含 \r\n）
    const char *expect_key; // 期望在回复中匹配的关键词（如 "OK"、"WIFI GOT IP"）
                            // 若为 NULL，则只检测没有 ERROR/FAIL 就算成功

    // ---- 超时控制 ----
    uint32_t start_tick;    // 开始等待时刻（xTaskGetTickCount()）
    uint32_t timeout_ticks; // 等待超时时间（pdMS_TO_TICKS(ms)）

    // ---- 内部标志 ----
    bool reply_handled; // 防止重复处理同一行
} at_cmd_ctx_t;

at_state_t net_wifi_at_cmd_process(at_cmd_ctx_t *ctx, uint8_t *rx_buf, uint32_t rx_buf_size);
at_state_t net_wifi_mode_set(uint8_t *rx_buf, uint32_t rx_buf_size);
void net_wifi_connect(void);

#endif // NET_WIFI_H