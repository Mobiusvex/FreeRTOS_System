// bsp_uart.h
#ifndef __BSP_UART_H
#define __BSP_UART_H

#include "sys_defs.h" // 统一状态码 SYS_OK, SYS_TIMEOUT...
#include <stdint.h>
#include <stdbool.h>
#include "cmsis_os2.h"

// 1. 定义系统中有哪些UART实例
typedef enum {
    BSP_UART_ESP8266, // 接ESP8266 (AT指令)
    BSP_UART_NUMBER
} BSP_UART_Bus_t;

#define BSP_UART_ESP8266_EVENT_MASK (1 << 0) // 用于ESP8266的事件标志

// 2. 配置结构体（可扩展）
typedef struct {
    uint32_t baudrate;       // 如 115200
    uint8_t word_length;     // 8位数据位
    uint8_t stop_bits;       // 1位停止位
    uint8_t parity;          // 无校验
    bool use_dma_rx;         // 是否开启DMA接收（强烈建议开启）
    uint16_t rx_buffer_size; // 接收缓冲区大小（环形缓冲区）
} BSP_UART_Config_t;

// 3. 对外核心API
SYS_StatusTypeDef BSP_UART_Init(BSP_UART_Bus_t bus, const BSP_UART_Config_t *cfg);

// 发送（阻塞/带超时）
SYS_StatusTypeDef BSP_UART_Transmit(BSP_UART_Bus_t bus, const uint8_t *data, uint16_t len, uint32_t timeout_ms);

// 接收（阻塞/带超时）——主要用于与ESP8266一问一答
SYS_StatusTypeDef BSP_UART_Receive(BSP_UART_Bus_t bus, uint8_t *data, uint16_t len, uint32_t timeout_ms);

// ★ 非阻塞读取（从DMA/中断环形缓冲区中读走数据，不阻塞CPU）
uint16_t BSP_UART_ReadFromBuffer(BSP_UART_Bus_t bus, uint8_t *buffer, uint16_t max_len);

// ★ 查询接收缓冲区现有数据长度
uint16_t BSP_UART_GetRxCount(BSP_UART_Bus_t bus);

osEventFlagsId_t BSP_UART_GetEventGroup(void);

// ========== 发送接口（新增） ==========

SYS_StatusTypeDef BSP_UART_Transmit_IT(BSP_UART_Bus_t bus, const uint8_t *data, uint16_t len);

SYS_StatusTypeDef BSP_UART_Transmit_Block(BSP_UART_Bus_t bus, const uint8_t *data, uint16_t len, uint32_t timeout_ms);

bool BSP_UART_IsTxIdle(BSP_UART_Bus_t bus);

#endif