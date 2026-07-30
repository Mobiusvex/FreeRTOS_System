// bsp_spi.c
#include "bsp_spi.h"
#include "bsp_gpio.h" // 软件SPI需要控制GPIO
#include "stm32f1xx_hal.h"
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
        .sw_delay_us = 2, // 约 500kHz 速率
    },
};

// 3. 初始化函数
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
        // // 硬件SPI：配置并调用HAL
        // SPI_HandleTypeDef *hspi = priv->hw_handle;
        // hspi->Init.CLKPhase = cfg->cpha;
        // hspi->Init.CLKPol = cfg->cpol;
        // hspi->Init.DataSize = SPI_DATASIZE_8BIT;
        // hspi->Init.FirstBit = SPI_FIRSTBIT_MSB;
        // hspi->Init.TIMode = SPI_TIMODE_DISABLE;
        // hspi->Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
        // hspi->Init.NSS = SPI_NSS_SOFT; // 硬件不控制CS，由Device层通过GPIO控制

        // // 计算分频系数 (简化版，实际需匹配系统时钟)
        // // 这里假设系统时钟72MHz，粗略映射
        // if (cfg->baudrate >= 18000000)
        //     hspi->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
        // else if (cfg->baudrate >= 9000000)
        //     hspi->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
        // else if (cfg->baudrate >= 4000000)
        //     hspi->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
        // else
        //     hspi->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;

        // return (HAL_SPI_Init(hspi) == HAL_OK);
    }
    return false;
}

/**
 * @brief  用于 XPT2046 的简单微秒级延时函数
 * @param  nCount ：延时计数值，单位为微妙
 * @retval 无
 */
void BSP_DelayUS(__IO uint32_t ulCount) {
    uint32_t i;

    for (i = 0; i < ulCount; i++) {
        uint8_t uc = 12; // 设置值为12，大约延1微秒

        while (uc--); // 延1微秒
    }
}

// TODO:改注释
/**
 * @brief 软件SPI全双工收发一个字节 (模式0: CPOL=0, CPHA=0)
 * @param tx_data 要发送的字节
 * @return 接收到的字节
 */
uint8_t SOFT_SPI_TransmitReceive(SPI_Private_t *priv, uint8_t tx_data) {
    uint8_t rx_data = 0;
    BSP_GPIO_Write(priv->sw_mosi, BSP_GPIO_LOW);
    BSP_GPIO_Write(priv->sw_sck, BSP_GPIO_LOW);
    // 循环处理8个位
    for (int i = 7; i >= 0; i--) {
        // 1. 主机根据要发送的位，设置MOSI引脚
        if (tx_data & (1 << i)) {
            BSP_GPIO_Write(priv->sw_mosi, BSP_GPIO_HIGH);
        } else {
            BSP_GPIO_Write(priv->sw_mosi, BSP_GPIO_LOW);
        }
        BSP_DelayUS(5);
        // 3. 读取从机在此时输出的位 (MISO)
        if (BSP_GPIO_Read(priv->sw_miso)) {
            rx_data |= (1 << i);
        }
        // 2. 拉高时钟，产生上升沿 (模式3，主机在上升沿采样从机数据)
        BSP_GPIO_Write(priv->sw_sck, BSP_GPIO_HIGH);
        BSP_DelayUS(5);
        // 4. 拉低时钟，完成一个周期的传输 (下降沿时，从机锁存主机的MOSI数据)
        BSP_GPIO_Write(priv->sw_sck, BSP_GPIO_LOW);
    }
    BSP_GPIO_Write(priv->sw_sck, BSP_GPIO_HIGH);
    return rx_data;
}

void SOFT_SPI_Write(SPI_Private_t *priv, uint8_t tx_data) {
    SOFT_SPI_TransmitReceive(priv, tx_data);
}
uint8_t SOFT_SPI_Read(SPI_Private_t *priv) {
    // 发送0xFF提供时钟，以读取从机数据
    return SOFT_SPI_TransmitReceive(priv, 0x00);
}

// 4. 内部软件SPI收发函数 (模式3为例，可根据CPOL/CPHA扩充)
static bool SW_SPI_TransmitReceive(SPI_Private_t *priv,
                                   const uint8_t *tx_data,
                                   uint8_t *rx_data,
                                   uint16_t length) {
    // 假设使用 Mode 0 (CPOL=0, CPHA=0) : 空闲低，上升沿采样
    for (uint16_t i = 0; i < length; i++) {
        uint8_t rx_byte = SOFT_SPI_TransmitReceive(priv, tx_data[i]);
        if (rx_data) rx_data[i] = rx_byte;
    }
    return true;
}
// 5. 统一对外收发接口 (带互斥锁)
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

    bool result = false;
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
