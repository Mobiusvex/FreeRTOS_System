#include "bsp_iic.h"
#include "bsp_gpio.h"
#include "bsp_delay.h"

typedef struct {
    bool is_software;
    // 硬件相关
    // SPI_HandleTypeDef *hw_handle;
    // 软件相关 (映射到 bsp_gpio 的逻辑引脚)
    BSP_GPIO_Name_t sw_scl;
    BSP_GPIO_Name_t sw_sda;
    uint16_t device_addr; // 设备地址
    uint32_t sw_delay_us; // 控制速率
    // 互斥锁 (软硬共用)
    // OSAL_MutexHandle_t mutex;
} IIC_Private_t;

static IIC_Private_t s_iic_bus[BSP_IIC_BUS_NUMBER] = {
    [BSP_IIC_BUS_MPU6050_SW] = {
        .is_software = true,
        .sw_scl = BSP_GPIO_MPU6050_SCL,
        .sw_sda = BSP_GPIO_MPU6050_SDA,
        .device_addr = 0x68, // MPU6050 I2C地址
        .sw_delay_us = 1,
    },
};

/**
 * @brief 控制 SCL 时钟线输出电平
 * @param priv iic总线私有数据
 * @param BitValue 目标电平状态（0 或 1）
 * @retval 无
 */
void SOFT_IIC_W_SCL(IIC_Private_t *priv, uint8_t BitValue) {
    if (BitValue == 0) {
        BSP_GPIO_Write(priv->sw_scl, BSP_GPIO_LOW);
    } else {
        BSP_GPIO_Write(priv->sw_scl, BSP_GPIO_HIGH);
    }
}

/**
 * @brief 控制 SDA 数据线输出电平
 * @param priv iic总线私有数据
 * @param BitValue 目标电平状态（0 或 1）
 */
void SOFT_IIC_W_SDA(IIC_Private_t *priv, uint8_t BitValue) {
    if (BitValue == 0) {
        BSP_GPIO_Write(priv->sw_sda, BSP_GPIO_LOW);
    } else {
        BSP_GPIO_Write(priv->sw_sda, BSP_GPIO_HIGH);
    }
}

/**
 * @brief 读取 SDA 数据线当前输入电平
 * @param priv iic总线私有数据
 * @retval 引脚当前逻辑电平（0 或 1）
 */
uint8_t SOFT_IIC_R_SDA(IIC_Private_t *priv) {
    uint8_t BitValue;
    BitValue = BSP_GPIO_Read(priv->sw_sda); // 读取 SDA 当前瞬态电平
    // BSP_DelayUS(priv->sw_delay_us);
    return BitValue;
}

/**
 * @brief 产生 I2C 起始信号
 * @param priv iic总线私有数据
 * @retval 无
 */
void SOFT_IIC_Start(IIC_Private_t *priv) {
    SOFT_IIC_W_SDA(priv, 1);        // 确保 SDA 初始状态为释放态
    SOFT_IIC_W_SCL(priv, 1);        // 确保 SCL 初始状态为释放态
    BSP_DelayUS(priv->sw_delay_us); // 延时 10us，控制总线频率不超过器件支持上限

    SOFT_IIC_W_SDA(priv, 0); // 核心动作：在 SCL 高电平期间拉低 SDA
    BSP_DelayUS(priv->sw_delay_us);

    SOFT_IIC_W_SCL(priv, 0); // 钳住总线，为后续发送数据位做准备
    BSP_DelayUS(priv->sw_delay_us);
}

/**
 * @brief：产生 I2C 终止信号
 * @param priv iic总线私有数据
 * @retval 无
 */
void SOFT_IIC_Stop(IIC_Private_t *priv) {
    SOFT_IIC_W_SCL(priv, 0); // 释放 SCL 至高电平
    BSP_DelayUS(priv->sw_delay_us);

    SOFT_IIC_W_SDA(priv, 0); // 确保 SDA 初始状态为拉低态
    BSP_DelayUS(priv->sw_delay_us);

    SOFT_IIC_W_SCL(priv, 1); // 释放 SCL 至高电平
    BSP_DelayUS(priv->sw_delay_us);

    SOFT_IIC_W_SDA(priv, 1); // 核心动作：在 SCL 高电平期间释放 SDA 产生上升沿
    BSP_DelayUS(priv->sw_delay_us);
}

/**
 * @brief 发送一个字节数据到总线
 * @param priv iic总线私有数据
 * @param Byte 待发送的 8 位无符号数据
 * @retval 无
 */
void SOFT_IIC_SendByte(IIC_Private_t *priv, uint8_t Byte) {
    uint8_t i;
    for (i = 0; i < 8; i++) {
        SOFT_IIC_W_SCL(priv, 0);
        BSP_DelayUS(priv->sw_delay_us);

        SOFT_IIC_W_SDA(priv, (Byte & (0x80 >> i)));
        BSP_DelayUS(priv->sw_delay_us);

        SOFT_IIC_W_SCL(priv, 1);
        BSP_DelayUS(priv->sw_delay_us);
    }
    SOFT_IIC_W_SCL(priv, 0);
}

/**
 * @brief 从IIC总线上接收一个字节数据
 * @param priv iic总线私有数据
 * @retval 从总线上重组的 8 位无符号数据
 */
uint8_t SOFT_IIC_ReceiveByte(IIC_Private_t *priv) {
    uint8_t i, Byte = 0x00; // 接收容器必须初始化清零

    SOFT_IIC_W_SDA(priv, 1);
    for (i = 0; i < 8; i++) {
        SOFT_IIC_W_SCL(priv, 0);
        BSP_DelayUS(priv->sw_delay_us);

        SOFT_IIC_W_SCL(priv, 1);
        BSP_DelayUS(priv->sw_delay_us);

        if (SOFT_IIC_R_SDA(priv)) // 检测 SDA 当前瞬态电平
        {
            Byte |= (0x80 >> i); // 若为高，则将 Byte 对应位掩码置 1
        }
    }
    SOFT_IIC_W_SCL(priv, 0);

    return Byte;
}

/**
 * @brief: 主机向从机发送应答位
 * @param priv iic总线私有数据
 */
void SOFT_IIC_SendAck(IIC_Private_t *priv, uint8_t AckBit) {
    SOFT_IIC_W_SCL(priv, 0); // 结束应答周期
    BSP_DelayUS(priv->sw_delay_us);

    SOFT_IIC_W_SDA(priv, AckBit); // 提前将应答电平放置于总线
    BSP_DelayUS(priv->sw_delay_us);

    SOFT_IIC_W_SCL(priv, 1); // 供从机采样
    BSP_DelayUS(priv->sw_delay_us);

    SOFT_IIC_W_SCL(priv, 0); // 结束应答周期
    BSP_DelayUS(priv->sw_delay_us);
}

/**
 * @brief: 主机读取从机的应答位反馈
 * @param priv iic总线私有数据
 * @retval 0表示收到应答(ACK)，1表示收到非应答(NACK)
 */
SYS_StatusTypeDef SOFT_IIC_ReceiveAck(IIC_Private_t *priv) {
    uint8_t AckBit;
    SOFT_IIC_W_SCL(priv, 0);
    BSP_DelayUS(priv->sw_delay_us);

    SOFT_IIC_W_SDA(priv, 1); // 主机释放数据线
    BSP_DelayUS(priv->sw_delay_us);

    SOFT_IIC_W_SCL(priv, 1);
    BSP_DelayUS(priv->sw_delay_us);

    AckBit = SOFT_IIC_R_SDA(priv); // 捕获电平状态

    SOFT_IIC_W_SCL(priv, 0);
    if (AckBit == 0) {
        return SYS_OK;
    } else {
        return SYS_NO_ACK;
    }
}

/**
 * @brief: 主机向从机发送数据
 * @param priv iic总线私有数据
 * @param bufp 待发送的缓冲区指针
 * @param len 待发送的字节数
 * @retval SYS_OK表示成功，SYS_NO_ACK表示未收到应答
 */
SYS_StatusTypeDef SOFT_I2C_Mem_Write(IIC_Private_t *priv, uint8_t *bufp, uint16_t len) {
    SOFT_IIC_Start(priv); // 通信开始信号

    SOFT_IIC_SendByte(priv, priv->device_addr << 1); // 发送IIC从机设备写地址
    if (SOFT_IIC_ReceiveAck(priv))                   // 主机接受从机ACK确认信号
    {
        SOFT_IIC_Stop(priv);
        return SYS_NO_ACK;
    }

    for (uint16_t i = 0; i < len; i++) // 循环发送length个字节长度的数据
    {
        SOFT_IIC_SendByte(priv, bufp[i]);
        if (SOFT_IIC_ReceiveAck(priv)) // 主机接受从机ACK确认信号
        {
            SOFT_IIC_Stop(priv);
            return SYS_NO_ACK;
        }
    }
    SOFT_IIC_Stop(priv); // 通信结束信号
    return SYS_OK;
}

/**
 * @brief: 主机从从机读取数据
 * @param priv iic总线私有数据
 * @param reg 要读取的寄存器地址
 * @param reg_size 寄存器地址的大小，1或2
 * @param bufp 接收缓冲区指针
 * @param len 待读取的字节数
 * @retval SYS_OK表示成功，SYS_NO_ACK表示未收到应答
 * @retval SYS_INVALID_PARAM表示寄存器地址大小不正确
 */
SYS_StatusTypeDef SOFT_I2C_Mem_Read(IIC_Private_t *priv, uint16_t reg, uint16_t reg_size, uint8_t *bufp, uint16_t len) {
    if (reg_size != 1 && reg_size != 2) {
        return SYS_INVALID_PARAM;
    }
    SOFT_IIC_Start(priv); // 通信开始信号

    SOFT_IIC_SendByte(priv, priv->device_addr << 1); // 主机发送IIC从机设备写地址
    if (SOFT_IIC_ReceiveAck(priv))                   // 主机接受从机ACK确认信号
    {
        SOFT_IIC_Stop(priv);
        return SYS_NO_ACK;
    }

    if (reg_size == 1) {                       // 8bit
        SOFT_IIC_SendByte(priv, (reg & 0xFF)); // 主机发送addr，要操作的寄存器地址
        if (SOFT_IIC_ReceiveAck(priv))         // 主机接受从机ACK确认信号
        {
            SOFT_IIC_Stop(priv);
            return SYS_NO_ACK;
        }
    } else {                                          // 16bit
        SOFT_IIC_SendByte(priv, ((reg >> 8) & 0xFF)); // 主机发送addr，要操作的寄存器地址
        if (SOFT_IIC_ReceiveAck(priv))                // 主机接受从机ACK确认信号
        {
            SOFT_IIC_Stop(priv);
            return SYS_NO_ACK;
        }
        SOFT_IIC_SendByte(priv, (reg & 0xFF)); // 主机发送addr，要操作的寄存器地址
        if (SOFT_IIC_ReceiveAck(priv))         // 主机接受从机ACK确认信号
        {
            SOFT_IIC_Stop(priv);
            return SYS_NO_ACK;
        }
    }

    SOFT_IIC_Start(priv); // 注意：IIC读字节需要重复发送通信开始信号

    SOFT_IIC_SendByte(priv, (priv->device_addr << 1) | 0X01); // 主机发送IIC从机设备读地址
    if (SOFT_IIC_ReceiveAck(priv))                            // 主机接受从机ACK确认信号
    {
        SOFT_IIC_Stop(priv);
        return SYS_NO_ACK;
    }

    for (uint16_t i = 0; i < len; i++) // 循环读取length个长度的字节数据
    {
        bufp[i] = SOFT_IIC_ReceiveByte(priv);
        if (i == len - 1) // 注意：读取到最后一个字节时，主机发送NACK信号
        {
            SOFT_IIC_SendAck(priv, 1); // 发送NACK信号结束
        } else                         // 注意：如果不是最后一个字节，主机发送ACK信号
            SOFT_IIC_SendAck(priv, 0);
    }

    SOFT_IIC_Stop(priv); // IIC通信结束

    return SYS_OK;
}

/**
 * @brief: IIC总线写操作
 * @param bus iic总线编号
 * @param pdata 待写入的缓冲区指针
 * @param len 待写入的字节数
 * @param timeout 超时时间（单位：ms）
 * @retval SYS_OK表示成功，SYS_TIMEOUT表示超时
 */
SYS_StatusTypeDef BSP_I2C_Write(BSP_IIC_Bus_t bus, uint8_t *pdata, uint16_t len, uint32_t timeout) {
    IIC_Private_t *priv = &s_iic_bus[bus];
    uint32_t start_time = HAL_GetTick();
    while (HAL_GetTick() - start_time < timeout) {
        if (priv->is_software) {
            if (SOFT_I2C_Mem_Write(priv, pdata, len) == SYS_OK) {
                return SYS_OK;
            }
        } else {
            // 硬件 SPI1
            // ...
        }
    }
    return SYS_TIMEOUT;
}

/**
 * @brief: IIC总线读操作
 * @param bus iic总线编号
 * @param mem_addr 要读取的寄存器地址
 * @param mem_addr_size 要读取的寄存器地址的大小，1或2
 * @param pdata 接收缓冲区指针
 * @param len 待读取的字节数
 * @param timeout 超时时间（单位：ms）
 * @retval SYS_OK表示成功，SYS_TIMEOUT表示超时,SYS_INVALID_PARAM表示寄存器地址大小不正确
 */
SYS_StatusTypeDef BSP_I2C_Read(BSP_IIC_Bus_t bus, uint16_t mem_addr, uint16_t mem_addr_size, uint8_t *pdata, uint16_t len, uint32_t timeout) {
    IIC_Private_t *priv = &s_iic_bus[bus];
    uint32_t start_time = HAL_GetTick();
    SYS_StatusTypeDef ret = SYS_OK;
    while (HAL_GetTick() - start_time < timeout) {
        if (priv->is_software) {
            // 软件模拟 SPI1
            ret = SOFT_I2C_Mem_Read(priv, mem_addr, 1, pdata, len);
            if (ret == SYS_OK || ret == SYS_INVALID_PARAM) {
                return ret;
            }
        } else {
            // 硬件 SPI1
            // ...
        }
    }
    return SYS_TIMEOUT;
}