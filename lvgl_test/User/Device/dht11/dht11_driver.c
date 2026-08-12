

#include "stm32f1xx_hal.h"
#include "dht11_driver.h"

#include "bsp_gpio.h"
#include "bsp_delay.h"

#include "SEGGER_RTT.h"

/* 控制GPIO读取DHT11的数据
 * 1. 主机发出至少18MS的低脉冲: start信号
 * 2. start信号变为高, 20-40us之后, dht11会拉低总线维持80us
      然后拉高80us: 回应信号
 * 3. 之后就是数据, 逐位发送
 *    bit0 : 50us低脉冲, 26-28us高脉冲
 *    bit1 : 50us低脉冲, 70us高脉冲
 * 4. 数据有40bit: 8bit湿度整数数据+8bit湿度小数数据
                   +8bit温度整数数据+8bit温度小数数据
                   +8bit校验和
 */

/**
 * @brief  配置DHT11的数据引脚为输出
 * @param  无
 * @retval 无
 */
static void DHT11_PinCfgAsOutput(void) {
    /* 对于STM32F103, 已经把DHT11的引脚配置为"open drain, pull-up" */
}

/**
 * @brief  配置DHT11的数据引脚为输入
 * @param  无
 * @retval 无
 */
static void DHT11_PinCfgAsInput(void) {
    /* 对于STM32F103, 已经把DHT11的引脚配置为"open drain, pull-up"
     * 让它输出1就不会驱动这个引脚, 并且可以读入引脚状态
     */
    BSP_GPIO_Write(BSP_GPIO_DHT11_DATA, BSP_GPIO_HIGH);
}

/**
 * @brief  设置DHT11的数据引脚的输出值
 * @param  val - 输出电平
 * @retval 无
 */
static void DHT11_PinSet(int val) {
    if (val)
        BSP_GPIO_Write(BSP_GPIO_DHT11_DATA, BSP_GPIO_HIGH);
    else
        BSP_GPIO_Write(BSP_GPIO_DHT11_DATA, BSP_GPIO_LOW);
}

/**
 * @brief  读取DHT11的数据引脚
 * @param  无
 * @retval 1-高电平, 0-低电平
 */
static int DHT11_PinRead(void) {
    if (BSP_GPIO_HIGH == BSP_GPIO_Read(BSP_GPIO_DHT11_DATA))
        return 1;
    else
        return 0;
}

/**
 * @brief  给DHT11发出启动信号
 * @param  无
 * @retval 无
 */
static void DHT11_Start(void) {
    DHT11_PinSet(0);
    BSP_DelayMS_Sleep(20);
    DHT11_PinCfgAsInput();
}

/**
 * @brief  等待DHT11的回应信号
 * @param  无
 * @retval 1-无响应, 0-有响应
 */
static int DHT11_Wait_Ack(void) {
    BSP_DelayUS(60);
    return DHT11_PinRead();
}

/**
 * @brief  在指定时间内等待数据引脚变为某个值
 * @param val - 期待数据引脚变为这个值
 *      timeout_us - 超时时间(单位us)
 * @retval 0-成功, (-1) - 失败
 */
static int DHT11_WaitFor_Val(int val, int timeout_us) {
    while (timeout_us--) {
        if (DHT11_PinRead() == val)
            return 0; /* ok */
        BSP_DelayUS(1);
    }
    return -1; /* err */
}

/**
 * @brief  读取DH11 1byte数据
 * @param  无
 * @retval 数据
 */
static int DHT11_ReadByte(void) {
    int i;
    int data = 0;

    for (i = 0; i < 8; i++) {
        if (DHT11_WaitFor_Val(1, 1000)) {
            // printf("dht11 wait for high data err!\n\r");
            return -1;
        }
        BSP_DelayUS(40);
        data <<= 1;
        if (DHT11_PinRead() == 1)
            data |= 1;

        if (DHT11_WaitFor_Val(0, 1000)) {
            // printf("dht11 wait for low data err!\n\r");
            return -1;
        }
    }

    return data;
}

/**
 * @brief  DHT11的初始化函数
 * @param  无
 * @retval 无
 */
void DHT11_Init(void) {
    DHT11_PinCfgAsOutput();
    DHT11_PinSet(1);
    // BSP_DelayMS(2000);
}

/**
 * @brief  读取DHT11的温度/湿度
 * @param  hum  - 用于保存湿度值
 *            temp - 用于保存温度值
 * @retval 0 - 成功, (-1) - 失败
 */
SYS_StatusTypeDef DHT11_Read(float *hum, float *temp) {
    uint8_t hum_m, hum_n;
    uint8_t temp_m, temp_n;
    uint8_t check;

    DHT11_Start();
    BSP_CRITICAL_Enter();
    if (0 != DHT11_Wait_Ack()) {
        // printf("dht11 not ack, err!\n\r");
        return SYS_ERROR;
    }

    if (0 != DHT11_WaitFor_Val(1, 1000)) /* 等待ACK变为高电平, 超时时间是1000us */
    {
        // printf("dht11 wait for ack high err!\n\r");
        return SYS_TIMEOUT;
    }

    if (0 != DHT11_WaitFor_Val(0, 1000)) /* 数据阶段: 等待低电平, 超时时间是1000us */
    {
        // printf("dht11 wait for data low err!\n\r");
        return SYS_TIMEOUT;
    }

    hum_m = DHT11_ReadByte();
    hum_n = DHT11_ReadByte();
    temp_m = DHT11_ReadByte();
    temp_n = DHT11_ReadByte();
    check = DHT11_ReadByte();
    BSP_CRITICAL_Exit(); // 退出临界区，恢复中断
    DHT11_PinCfgAsOutput();
    DHT11_PinSet(1);
    // SEGGER_RTT_printf(0, "hum_m=%d, hum_n=%d, temp_m=%d, temp_n=%d, check=%d\n\r", hum_m, hum_n, temp_m, temp_n, check);
    if (hum_m + hum_n + temp_m + temp_n == check) {
        *hum = (float)(hum_m * 10 + hum_n) / 10.0;
        *temp = (float)(temp_m * 10 + temp_n) / 10.0;
        return SYS_OK;
    } else {
        // printf("dht11 checksum err!\n\r");
        return SYS_INVALID_DATA;
    }
}
