// bsp_spi.c
#include "bsp_spi.h"
#include "bsp_gpio.h" // 软件SPI需要控制GPIO
#include "stm32f1xx_hal.h"
#include "bsp_delay.h"
#include "cmsis_os2.h"
#include "spi.h"

// 1. 内部总线属性结构体
typedef struct {
    bool is_software;
    // 硬件相关
    SPI_HandleTypeDef *hw_handle;
    // 软件相关 (映射到 bsp_gpio 的逻辑引脚)
    BSP_GPIO_Name_t sw_sck;
    BSP_GPIO_Name_t sw_mosi;
    BSP_GPIO_Name_t sw_miso;
    uint32_t sw_delay_us; // 控制速率
    // 互斥锁 (软硬共用)
    osMutexId_t mutex;
} SPI_Private_t;
// 2. 静态配置表
static SPI_Private_t s_spiBus[BSP_SPI_BUS_NUMBER] = {
    [BSP_SPI_BUS_XPT2046] = {
        .is_software = true,
        .sw_sck = BSP_GPIO_XPT2046_SPI_CLK, // 需在 bsp_gpio.h 中定义
        .sw_mosi = BSP_GPIO_XPT2046_SPI_MOSI,
        .sw_miso = BSP_GPIO_XPT2046_SPI_MISO,
        .sw_delay_us = 5,
    },
    [BSP_SPI_BUS_1] = {
        .is_software = false,
        // 硬件相关
        .hw_handle = &hspi1,
    }};

/**
 * @brief 初始化SPI总线
 * @param bus 要初始化的SPI总线号
 * @param cfg 总线配置参数
 * @return 初始化成功返回true，失败返回false
 */
bool BSP_SPI_Init(BSP_SPI_Bus_t bus, const BSP_SPI_Config_t *cfg) {
    if (bus >= BSP_SPI_BUS_NUMBER) return false;
    SPI_Private_t *priv = &s_spiBus[bus];

    if (priv->mutex == NULL) {
        priv->mutex = osMutexNew(NULL);
    }

    if (priv->is_software) {
        // 软件SPI：只需要保存延迟时间，GPIO已在 BSP_GPIO_Init 中配置为输出
        priv->sw_delay_us = (cfg->baudrate > 0) ? (1000000 / cfg->baudrate) : 2;
        // 设置空闲电平 (根据CPOL)
        BSP_GPIO_Write(priv->sw_sck, (cfg->cpol == 0) ? BSP_GPIO_LOW : BSP_GPIO_HIGH);
        return true;
    } else {
    }
    return false;
}

/**
 * @brief 软件SPI单字节收发
 * @param priv SPI总线私有数据
 * @param tx_data 要发送的数据
 * @retval 接收的数据
 */
static uint8_t SOFT_SPI_TransferByte(SPI_Private_t *priv, uint8_t tx_data) {
    uint8_t rx_data = 0;

    BSP_GPIO_Write(priv->sw_sck, BSP_GPIO_LOW);
    // 循环处理8个位
    for (int i = 7; i >= 0; i--) {
        if (tx_data & (1 << i)) {
            BSP_GPIO_Write(priv->sw_mosi, BSP_GPIO_HIGH);
        } else {
            BSP_GPIO_Write(priv->sw_mosi, BSP_GPIO_LOW);
        }
        BSP_DelayUS(priv->sw_delay_us);

        BSP_GPIO_Write(priv->sw_sck, BSP_GPIO_HIGH);
        if (BSP_GPIO_Read(priv->sw_miso)) {
            rx_data |= (1 << i);
        }
        BSP_DelayUS(priv->sw_delay_us);

        BSP_GPIO_Write(priv->sw_sck, BSP_GPIO_LOW);
    }
    BSP_GPIO_Write(priv->sw_sck, BSP_GPIO_HIGH);
    return rx_data;
}

/**
 * @brief 软件SPI发送数据
 * @param priv SPI总线私有数据
 * @param tx_data 要发送的数据缓冲区
 * @param length 要发送的数据长度
 * @retval 无
 */
void SOFT_SPI_Write(SPI_Private_t *priv, const uint8_t *tx_data, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        (void)SOFT_SPI_TransferByte(priv, tx_data[i]);
    }
}

/**
 * @brief 软件SPI接收数据
 * @param priv SPI总线私有数据
 * @param rx_data 接收的数据缓冲区
 * @param length 要接收的数据长度
 * @retval 无
 */
void SOFT_SPI_Read(SPI_Private_t *priv, uint8_t *rx_data, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        rx_data[i] = SOFT_SPI_TransferByte(priv, 0xFF);
    }
}

/**
 * @brief 软件SPI全双工收发数据
 * @param priv SPI总线私有数据
 * @param tx_data 要发送的数据缓冲区
 * @param rx_data 接收的数据缓冲区
 * @param length 要发送/接收的数据长度
 * @retval 无
 */
static bool SOFT_SPI_TransmitReceive(SPI_Private_t *priv,
                                     const uint8_t *tx_data,
                                     uint8_t *rx_data,
                                     uint16_t length) {
    for (uint16_t i = 0; i < length; i++) {
        uint8_t rx_byte = SOFT_SPI_TransferByte(priv, tx_data[i]);
        if (rx_data) rx_data[i] = rx_byte;
    }
    return true;
}

/**
 * @brief SPI全双工收发数据,对外接口
 * @param bus 要操作的SPI总线号
 * @param op 操作模式
 * @param cs 软件SPIP片选引脚
 * @param tx_data 要发送的数据缓冲区
 * @param rx_data 接收的数据缓冲区
 * @param length 要发送/接收的数据长度
 * @param timeout_ms 超时时间 (单位: 毫秒)
 * @retval false: 超时或错误 true: 成功
 */
bool spi_transfer_internal(BSP_SPI_Bus_t bus,
                           BSP_SPI_Op_t op,
                           BSP_GPIO_Name_t cs,
                           const uint8_t *tx_data, uint16_t tx_len,
                           uint8_t *rx_data, uint16_t rx_len,
                           uint32_t timeout_ms) {
    if (bus >= BSP_SPI_BUS_NUMBER) return false;

    /* 参数合法性检查 */
    switch (op) {
    case BSP_SPI_OP_TX_ONLY:
        if (tx_data == NULL || tx_len == 0) return false;
        break;
    case BSP_SPI_OP_RX_ONLY:
        if (rx_data == NULL || rx_len == 0) return false;
        break;
    case BSP_SPI_OP_TX_RX:
        if (tx_data == NULL || rx_data == NULL) return false;
        if (tx_len == 0 || tx_len != rx_len) return false;
        break;
    case BSP_SPI_OP_TX_THEN_RX:
        if (tx_data == NULL || tx_len == 0) return false;
        if (rx_data == NULL || rx_len == 0) return false;
        break;
    default:
        return false;
    }

    SPI_Private_t *priv = &s_spiBus[bus];
    if (!osMutexAcquire(priv->mutex, timeout_ms)) return false;
    if (cs != BSP_GPIO_NONE) {
        BSP_GPIO_Write(cs, BSP_GPIO_LOW);
    }
    bool ok = true;

    /* ---------- 软件模拟 ---------- */
    if (priv->is_software) {
        switch (op) {
        case BSP_SPI_OP_TX_ONLY:
            SOFT_SPI_Write(priv, tx_data, tx_len);
            break;

        case BSP_SPI_OP_RX_ONLY:
            SOFT_SPI_Read(priv, rx_data, rx_len);
            break;

        case BSP_SPI_OP_TX_RX:
            SOFT_SPI_TransmitReceive(priv, tx_data, rx_data, tx_len);
            break;

        case BSP_SPI_OP_TX_THEN_RX:
            SOFT_SPI_Write(priv, tx_data, tx_len);
            SOFT_SPI_Read(priv, rx_data, rx_len);
            break;
        }
    }
    /* ---------- 硬件 SPI ---------- */
    else {
        SPI_HandleTypeDef *h = priv->hw_handle;
        switch (op) {
        case BSP_SPI_OP_TX_ONLY:
            ok = (HAL_SPI_Transmit(h, (uint8_t *)tx_data, tx_len,
                                   timeout_ms)
                  == HAL_OK);
            break;
        case BSP_SPI_OP_RX_ONLY:
            ok = (HAL_SPI_Receive(h, rx_data, rx_len,
                                  timeout_ms)
                  == HAL_OK);
            break;
        case BSP_SPI_OP_TX_RX:
            ok = (HAL_SPI_TransmitReceive(h, (uint8_t *)tx_data, rx_data,
                                          tx_len, timeout_ms)
                  == HAL_OK);
            break;
        case BSP_SPI_OP_TX_THEN_RX:
            ok = (HAL_SPI_Transmit(h, (uint8_t *)tx_data, tx_len,
                                   timeout_ms)
                  == HAL_OK);
            if (ok) {
                ok = (HAL_SPI_Receive(h, rx_data, rx_len,
                                      timeout_ms)
                      == HAL_OK);
            }
            break;
        }
    }
    if (cs != BSP_GPIO_NONE) {
        BSP_GPIO_Write(cs, BSP_GPIO_HIGH); // 释放CS
    }
    osMutexRelease(priv->mutex);
    return ok;
}

/**
 * @brief SPI发送数据,对外接口
 * @param bus 要操作的SPI总线号
 * @param cs 软件SPIP片选引脚
 * @param tx 要发送的数据缓冲区
 * @param len 要发送/接收的数据长度
 * @param timeout_ms 超时时间 (单位: 毫秒)
 */
bool BSP_SPI_Write(BSP_SPI_Bus_t bus, BSP_GPIO_Name_t cs,
                   const uint8_t *tx, uint16_t len, uint32_t timeout_ms) {
    return spi_transfer_internal(bus, BSP_SPI_OP_TX_ONLY, cs,
                                 tx, len, NULL, 0, timeout_ms);
}

/**
 * @brief SPI接收数据,对外接口
 * @param bus 要操作的SPI总线号
 * @param cs 软件SPIP片选引脚
 * @param rx 接收的数据缓冲区
 * @param len 要发送/接收的数据长度
 * @param timeout_ms 超时时间 (单位: 毫秒)
 */
bool BSP_SPI_Read(BSP_SPI_Bus_t bus, BSP_GPIO_Name_t cs,
                  uint8_t *rx, uint16_t len, uint32_t timeout_ms) {
    return spi_transfer_internal(bus, BSP_SPI_OP_RX_ONLY, cs,
                                 NULL, 0, rx, len, timeout_ms);
}

/**
 * @brief SPI全双工发送接收数据,对外接口
 * @param bus 要操作的SPI总线号
 * @param cs 软件SPIP片选引脚
 * @param tx 接收的数据缓冲区
 * @param rx 接收的数据缓冲区
 * @param len 要发送/接收的数据长度
 * @param timeout_ms 超时时间 (单位: 毫秒)
 */
bool BSP_SPI_WriteRead(BSP_SPI_Bus_t bus, BSP_GPIO_Name_t cs,
                       const uint8_t *tx, uint8_t *rx,
                       uint16_t len, uint32_t timeout_ms) {
    return spi_transfer_internal(bus, BSP_SPI_OP_TX_RX, cs,
                                 tx, len, rx, len, timeout_ms);
}

/**
 * @brief SPI半双工先发后接收数据,对外接口
 * @param bus 要操作的SPI总线号
 * @param cs 软件SPIP片选引脚
 * @param tx 接收的数据缓冲区
 * @param tx_len 要发送的数据长度
 * @param rx 接收的数据缓冲区
 * @param rx_len 接收的数据长度
 * @param timeout_ms 超时时间 (单位: 毫秒)
 */
bool BSP_SPI_WriteThenRead(BSP_SPI_Bus_t bus, BSP_GPIO_Name_t cs,
                           const uint8_t *tx, uint16_t tx_len,
                           uint8_t *rx, uint16_t rx_len,
                           uint32_t timeout_ms) {
    return spi_transfer_internal(bus, BSP_SPI_OP_TX_THEN_RX, cs,
                                 tx, tx_len, rx, rx_len, timeout_ms);
}