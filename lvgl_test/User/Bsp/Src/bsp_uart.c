// bsp_uart.c
#include "bsp_uart.h"
#include "bsp_gpio.h" // 如果需要复用/翻转引脚
#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include "usart.h"
#include "dma.h"
#include "SEGGER_RTT.h"

extern DMA_HandleTypeDef hdma_usart3_rx;
extern DMA_HandleTypeDef hdma_usart1_rx;

// ---------- 私有上下文 ----------
typedef struct {
    UART_HandleTypeDef *huart; // CubeMX生成的句柄
    DMA_HandleTypeDef *hdmarx; // CubeMX生成的句柄
    bool use_dma_rx;
    // 环形缓冲区（软件FIFO）
    uint8_t *rx_buffer; // 指向分配的内存
    uint16_t rx_size;
    volatile uint16_t rx_tail; // 写入索引（中断/DMA中更新）
    volatile uint16_t rx_head; // 读取索引（用户调用读取时更新）
    // 发送
    volatile bool tx_busy;     // 发送忙标志
    volatile bool tx_complete; // 发送完成标志（用于阻塞等待）
    osSemaphoreId_t tx_sem;    // 信号量（用于阻塞等待发送完成）
    osThreadId_t rx_task_handle;

} UART_Ctx_t;

// 静态实例（内存分配由你决定，这里演示静态数组）
#define BSP_UART_BUFFER_SIZE 512
static uint8_t s_uart_rx_buf[BSP_UART_NUMBER][BSP_UART_BUFFER_SIZE];

static UART_Ctx_t s_uartCtx[BSP_UART_NUMBER] = {
    [BSP_UART_ESP8266] = {
        .huart = &huart3,
        .hdmarx = &hdma_usart3_rx,
        .use_dma_rx = true, // ESP8266数据量大，开DMA
        .rx_buffer = s_uart_rx_buf[BSP_UART_ESP8266],
        .rx_size = BSP_UART_BUFFER_SIZE,
        .tx_busy = false,
        .tx_complete = false,
        .rx_task_handle = NULL, // 用于接收任务
    },
    [BSP_UART_PC] = {
        .huart = &huart1,
        .hdmarx = &hdma_usart1_rx,
        .use_dma_rx = true,
        .rx_buffer = s_uart_rx_buf[BSP_UART_PC],
        .rx_size = BSP_UART_BUFFER_SIZE,
        .tx_busy = false,
        .tx_complete = false,
        .rx_task_handle = NULL, // 用于接收任务

    }};

/**
 * @brief 初始化UART
 * @param bus      UART总线
 * @param cfg      配置参数（NULL表示使用CubeMX生成的默认配置）
 * @retval SYS_OK: 初始化成功, SYS_INVALID_PARAM: 参数无效, SYS_ERROR: 初始化失败
 */
SYS_StatusTypeDef BSP_UART_Init(BSP_UART_Bus_t bus, const BSP_UART_Config_t *cfg) {
    if (bus >= BSP_UART_NUMBER) return SYS_INVALID_PARAM;

    // 获取HAL句柄
    UART_Ctx_t *ctx = &s_uartCtx[bus];
    UART_HandleTypeDef *huart = ctx->huart;
    DMA_HandleTypeDef *hdmarx = ctx->hdmarx;

    // 配置UART参数（HAL结构体赋值）
    if (cfg != NULL) {
        huart->Init.BaudRate = cfg->baudrate;
        huart->Init.WordLength = cfg->word_length;
        huart->Init.StopBits = cfg->stop_bits;
        huart->Init.Parity = cfg->parity;
        huart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
        huart->Init.Mode = UART_MODE_TX_RX;

        if (HAL_UART_Init(huart) != HAL_OK) return SYS_ERROR;
    }

    // 初始化发送环形缓冲区
    ctx->rx_head = 0;
    ctx->rx_tail = 0;
    memset((void *)ctx->rx_buffer, 0, ctx->rx_size);

    // 初始化发送状态
    ctx->tx_busy = false;
    ctx->tx_complete = false;

    // 启动接收（DMA模式）
    if (ctx->use_dma_rx) {
        // 启动DMA空闲中断接收（经典做法：空闲中断+DMA）
        //__HAL_UART_ENABLE_IT(huart, UART_IT_IDLE); // 使能空闲中断
        if (HAL_UARTEx_ReceiveToIdle_DMA(huart, (uint8_t *)ctx->rx_buffer, ctx->rx_size) != HAL_OK) {
            return SYS_ERROR;
        }
        __HAL_DMA_DISABLE_IT(hdmarx, DMA_IT_HT);
        __HAL_DMA_DISABLE_IT(hdmarx, DMA_IT_TC);
        // 注意：需要在HAL_UART_RxCpltCallback中处理DMA半满/全满，这里暂略。
    } else {
        // 中断接收每个字节（适用于调试口或小数据）
        HAL_UART_Receive_IT(huart, (uint8_t *)ctx->rx_buffer, 1); // 每次收1字节进中断
    }

    // 创建二值信号量（初始为0）
    ctx->tx_sem = osSemaphoreNew(1, 0, NULL);
    if (ctx->tx_sem == NULL) {
        return SYS_ERROR;
    }

    return SYS_OK;
}

void BSP_UART_RegisterTask(BSP_UART_Bus_t bus, osThreadId_t task) {
    if (bus < BSP_UART_NUMBER) {
        s_uartCtx[bus].rx_task_handle = task;
    }
}

/**
 * @brief 发送数据（非阻塞）
 * @param bus      UART总线
 * @param data     数据缓冲区
 * @param len      数据长度
 * @param timeout_ms 超时时间（毫秒）
 * @retval SYS_OK: 发送成功, SYS_TIMEOUT: 超时, SYS_ERROR: 其他错误
 */
SYS_StatusTypeDef BSP_UART_Transmit(BSP_UART_Bus_t bus, const uint8_t *data, uint16_t len, uint32_t timeout_ms) {
    if (bus >= BSP_UART_NUMBER || data == NULL || len == 0) return SYS_INVALID_PARAM;
    UART_Ctx_t *ctx = &s_uartCtx[bus];

    // 加互斥锁，防止多任务乱发数据（例如Debug和ESP8266同时发）
    // osMutexAcquire(ctx->tx_mutex, timeout_ms);

    HAL_StatusTypeDef status = HAL_UART_Transmit(ctx->huart, (uint8_t *)data, len, timeout_ms);

    // osMutexRelease(ctx->tx_mutex);

    return (status == HAL_OK) ? SYS_OK : (status == HAL_TIMEOUT ? SYS_TIMEOUT : SYS_ERROR);
}

/**
 * @brief 读取缓冲区数据（非阻塞）
 * @param bus      UART总线
 * @param buffer  数据缓冲区
 * @param max_len 最大读取长度
 * @retval 读取到的字节数
 */
uint16_t BSP_UART_ReadFromBuffer(BSP_UART_Bus_t bus, uint8_t *buffer, uint16_t max_len) {
    UART_Ctx_t *ctx = &s_uartCtx[bus];
    uint16_t head, tail;
    uint16_t available, copy_len, cont_len;

    taskENTER_CRITICAL();
    head = ctx->rx_head;
    tail = ctx->rx_tail;
    taskEXIT_CRITICAL();

    if (head <= tail) {
        available = tail - head;
    } else {
        available = ctx->rx_size - head + tail;
    }

    if (available == 0) {
        return 0;
    }
    copy_len = (max_len < available) ? max_len : available;

    cont_len = ctx->rx_size - head;
    if (copy_len <= cont_len) {
        memcpy(buffer, &ctx->rx_buffer[head], copy_len);
    } else {
        memcpy(buffer, &ctx->rx_buffer[head], cont_len);
        memcpy(buffer + cont_len, ctx->rx_buffer, copy_len - cont_len);
    }

    taskENTER_CRITICAL();
    ctx->rx_head = tail;
    taskEXIT_CRITICAL();

    return copy_len;
}

/**
 * @brief 中断方式发送数据（非阻塞）
 * @param bus      UART总线
 * @param data     数据缓冲区
 * @param len      数据长度
 * @retval SYS_OK: 启动成功, SYS_BUSY: 上一次发送未完成
 */
SYS_StatusTypeDef BSP_UART_Transmit_IT(BSP_UART_Bus_t bus, const uint8_t *data, uint16_t len) {
    if (bus >= BSP_UART_NUMBER || data == NULL || len == 0) {
        return SYS_INVALID_PARAM;
    }

    UART_Ctx_t *ctx = &s_uartCtx[bus];

    // 关闭调度器
    uint32_t lock_state = osKernelLock();
    // ★ 如果上一次发送未完成，拒绝新请求
    if (ctx->tx_busy) {
        osKernelRestoreLock(lock_state);
        return SYS_BUSY;
    }

    // 标记忙
    ctx->tx_busy = true;
    ctx->tx_complete = false;
    osKernelRestoreLock(lock_state);

    // 调用 HAL 中断发送
    HAL_StatusTypeDef status = HAL_UART_Transmit_IT(ctx->huart, (uint8_t *)data, len);

    if (status != HAL_OK) {
        lock_state = osKernelLock();
        ctx->tx_busy = false;
        ctx->tx_complete = false;
        osKernelRestoreLock(lock_state);
        return SYS_ERROR;
    }

    return SYS_OK;
}

/**
 * @brief 阻塞方式发送数据（等待发送完成）
 * @param bus      UART总线
 * @param data     数据缓冲区
 * @param len      数据长度
 * @param timeout_ms 超时时间（毫秒）
 * @retval SYS_OK: 发送成功, SYS_TIMEOUT: 超时, SYS_ERROR: 其他错误
 */
SYS_StatusTypeDef BSP_UART_Transmit_Block(BSP_UART_Bus_t bus, const uint8_t *data, uint16_t len, uint32_t timeout_ms) {
    // 1. 先尝试用中断方式启动发送
    SYS_StatusTypeDef ret = BSP_UART_Transmit_IT(bus, data, len);
    if (ret != SYS_OK) {
        return ret; // 启动失败（可能是繁忙或参数错误）
    }

    // 2. 等待信号量（发送完成回调中释放）
    UART_Ctx_t *ctx = &s_uartCtx[bus];
    if (osSemaphoreAcquire(ctx->tx_sem, timeout_ms) == osOK) {
        return SYS_OK;
    } else {
        // 超时：取消发送（HAL_UART_Abort_IT），将状态复位
        HAL_UART_Abort_IT(ctx->huart);
        ctx->tx_busy = false;
        return SYS_TIMEOUT;
    }
}

/**
 * @brief 接收中断回调函数
 */
void HAL_UARTEx_RxIDLECallback(UART_HandleTypeDef *huart) {
    // 查找对应的总线
    for (int i = 0; i < BSP_UART_NUMBER; i++) {
        if (s_uartCtx[i].huart == huart) {
            UART_Ctx_t *ctx = &s_uartCtx[i];
            uint16_t write_idx = ctx->rx_size - __HAL_DMA_GET_COUNTER(huart->hdmarx);

            if (write_idx == ctx->rx_size) {
                write_idx = 0;
            }
            ctx->rx_tail = write_idx; // 直接赋值当前写位置
            osThreadFlagsSet(ctx->rx_task_handle, 0x01);
            break;
        }
    }
}

/**
 * @brief 发送完成回调函数
 * @param huart UART句柄
 * @retval 无
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    // 查找对应的总线
    for (int i = 0; i < BSP_UART_NUMBER; i++) {
        if (s_uartCtx[i].huart == huart) {
            UART_Ctx_t *ctx = &s_uartCtx[i];

            // 1. 清除忙标志
            ctx->tx_busy = false;
            ctx->tx_complete = true;

            // 2. ★ 释放信号量（唤醒等待发送完成的阻塞任务）
            osSemaphoreRelease(ctx->tx_sem);

            break;
        }
    }
}

/**
 * @brief 发送错误回调函数
 * @param huart UART句柄
 * @retval 无
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    // 同样需要复位状态，并释放信号量（传递错误信号）
    for (int i = 0; i < BSP_UART_NUMBER; i++) {
        if (s_uartCtx[i].huart == huart) {
            UART_Ctx_t *ctx = &s_uartCtx[i];
            ctx->tx_busy = false;
            ctx->tx_complete = false;
            // 重启DMA
            HAL_UART_AbortReceive(huart);
            __HAL_UART_CLEAR_OREFLAG(huart);
            HAL_UARTEx_ReceiveToIdle_DMA(huart, (uint8_t *)ctx->rx_buffer, ctx->rx_size);
            __HAL_DMA_DISABLE_IT(ctx->hdmarx, DMA_IT_HT);
            __HAL_DMA_DISABLE_IT(ctx->hdmarx, DMA_IT_TC);
            // 可以选择释放信号量，并让上层检测到超时
            break;
        }
    }
}

/**
 * @brief 查询当前发送是否空闲
 * @param bus UART总线
 * @return true: 空闲可发送, false: 正在发送中
 */
bool BSP_UART_IsTxIdle(BSP_UART_Bus_t bus) {
    if (bus >= BSP_UART_NUMBER) return false;
    return !s_uartCtx[bus].tx_busy;
}