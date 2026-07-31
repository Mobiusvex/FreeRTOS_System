// bsp_spi.c
#include "bsp_spi.h"
#include "bsp_gpio.h" // 软件SPI需要控制GPIO
#include "stm32f1xx_hal.h"
#include "bsp_delay.h"
// #include "osal/osal_sync.h"

// 1. 内部总线属性结构体
typedef struct {
    bool is_software;
    // 硬件相关
    // SPI_HandleTypeDef *hw_handle;
    // 软件相关 (映射到 bsp_gpio 的逻辑引脚)
    BSP_GPIO_Name_t sw_sck;
    BSP_GPIO_Name_t sw_mosi;
    BSP_GPIO_Name_t sw_miso;
    uint32_t sw_delay_us; // 控制速率
    // 互斥锁 (软硬共用)
    // OSAL_MutexHandle_t mutex;
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
};

/**
 * @brief 初始化SPI总线
 * @param bus 要初始化的SPI总线号
 * @param cfg 总线配置参数
 * @return 初始化成功返回true，失败返回false
 */
bool BSP_SPI_Init(BSP_SPI_Bus_t bus, const BSP_SPI_Config_t *cfg) {
    if (bus >= BSP_SPI_BUS_NUMBER) return false;
    SPI_Private_t *priv = &s_spiBus[bus];

    // TODO:  创建互斥锁
    // if (priv->mutex == NULL) {
    //     priv->mutex = OSAL_Mutex_Create();
    // }

    if (priv->is_software) {
        // 软件SPI：只需要保存延迟时间，GPIO已在 BSP_GPIO_Init 中配置为输出
        priv->sw_delay_us = (cfg->baudrate > 0) ? (1000000 / cfg->baudrate) : 2;
        // 设置空闲电平 (根据CPOL)
        BSP_GPIO_Write(priv->sw_sck, (cfg->cpol == 0) ? BSP_GPIO_LOW : BSP_GPIO_HIGH);
        return true;
    } else {
        // TODO: 硬件SPI初始化
    }
    return false;
}

/**
 * @brief 软件SPI全双工收发一个字节 (模式3: CPOL=1, CPHA=1)
 * @param priv SPI总线私有数据
 * @param tx_data 要发送的字节
 * @return 接收到的字节
 */
uint8_t SOFT_SPI_TransmitReceive(SPI_Private_t *priv, uint8_t tx_data) {
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
 * @brief 软件SPI发送一个字节
 * @param priv SPI总线私有数据
 * @param tx_data 要发送的字节
 * @retval 无
 */
void SOFT_SPI_Write(SPI_Private_t *priv, uint8_t tx_data) {
    SOFT_SPI_TransmitReceive(priv, tx_data);
}

/**
 * @brief 软件SPI接收一个字节
 * @param priv SPI总线私有数据
 * @retval 接收到的字节
 */
uint8_t SOFT_SPI_Read(SPI_Private_t *priv) {
    // 发送0xFF提供时钟，以读取从机数据
    return SOFT_SPI_TransmitReceive(priv, 0x00);
}

/**
 * @brief 软件SPI全双工收发数据
 * @param priv SPI总线私有数据
 * @param tx_data 要发送的数据缓冲区
 * @param rx_data 接收的数据缓冲区
 * @param length 要发送/接收的数据长度
 * @retval 无
 */
static bool SW_SPI_TransmitReceive(SPI_Private_t *priv,
                                   const uint8_t *tx_data,
                                   uint8_t *rx_data,
                                   uint16_t length) {
    for (uint16_t i = 0; i < length; i++) {
        uint8_t rx_byte = SOFT_SPI_TransmitReceive(priv, tx_data[i]);
        if (rx_data) rx_data[i] = rx_byte;
    }
    return true;
}

/**
 * @brief SPI全双工收发数据,对外接口
 * @param bus 要操作的SPI总线号
 * @param tx_data 要发送的数据缓冲区
 * @param rx_data 接收的数据缓冲区
 * @param length 要发送/接收的数据长度
 * @param timeout_ms 超时时间 (单位: 毫秒)
 * @retval false: 超时或错误 true: 成功
 */
bool BSP_SPI_TransmitReceive(BSP_SPI_Bus_t bus,
                             const uint8_t *tx_data,
                             uint8_t *rx_data,
                             uint16_t length,
                             uint32_t timeout_ms) {
    if (bus >= BSP_SPI_BUS_NUMBER || length == 0) return false;
    SPI_Private_t *priv = &s_spiBus[bus];

    // TODO:  ① 获取互斥锁 (防止总线竞争)
    // if (!OSAL_Mutex_Take(priv->mutex, timeout_ms)) {
    //     return false;
    // }

    bool result = true;
    if (priv->is_software) {
        // ★ 调用软件模拟
        result = SW_SPI_TransmitReceive(priv, tx_data, rx_data, length);
    } else {
        // TODO:  ★ 调用 HAL 硬件
        // result = (HAL_SPI_TransmitReceive(priv->hw_handle,
        //                                   (uint8_t *)tx_data,
        //                                   rx_data,
        //                                   length,
        //                                   timeout_ms)
        //           == HAL_OK);
    }

    // TODO:  OSAL_Mutex_Give(priv->mutex);
    return result;
}
