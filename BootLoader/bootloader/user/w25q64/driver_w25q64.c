#include "driver_w25q64.h"
#include "main.h"
#include "spi.h"
#include <string.h>

/* =========================================================
 *  根据实际硬件修改以下 3 处
 * ========================================================= */
#define W25Q64_SPI_HANDLE (&hspi1) /* 如果是 SPI2，则改成 &hspi2 */
#define W25Q64_CS_GPIO_Port GPIOA  /* W25Q64 CS 端口 */
#define W25Q64_CS_Pin GPIO_PIN_4   /* W25Q64 CS 引脚 */

#define W25Q64_RETRY_MAX 3U
#define W25Q64_RETRY_DELAY_MS 1U

/* CS 控制 */
#define W25Q64_CS_LOW() HAL_GPIO_WritePin(W25Q64_CS_GPIO_Port, W25Q64_CS_Pin, GPIO_PIN_RESET)
#define W25Q64_CS_HIGH() HAL_GPIO_WritePin(W25Q64_CS_GPIO_Port, W25Q64_CS_Pin, GPIO_PIN_SET)

/* =========================================================
 *  HAL 版 SPI 写函数
 * ========================================================= */
static bool W25Q64_SPI_Write(uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
    HAL_StatusTypeDef ret;

    W25Q64_CS_LOW();
    ret = HAL_SPI_Transmit(W25Q64_SPI_HANDLE, pData, Size, Timeout);
    W25Q64_CS_HIGH();

    return (ret == HAL_OK);
}

/* =========================================================
 *  HAL 版 SPI 先写后读函数
 * ========================================================= */
static bool W25Q64_SPI_WriteThenRead(uint8_t *pTxData, uint16_t TxSize,
                                     uint8_t *pRxData, uint16_t RxSize,
                                     uint32_t Timeout)
{
    HAL_StatusTypeDef ret;

    W25Q64_CS_LOW();

    ret = HAL_SPI_Transmit(W25Q64_SPI_HANDLE, pTxData, TxSize, Timeout);
    if (ret == HAL_OK)
    {
        ret = HAL_SPI_Receive(W25Q64_SPI_HANDLE, pRxData, RxSize, Timeout);
    }

    W25Q64_CS_HIGH();

    return (ret == HAL_OK);
}

/* =========================================================
 *  写使能（0x06），并校验 WEL 位是否成功置位
 * ========================================================= */
bool BSP_W25Qxx_WriteEnable(void)
{
    uint8_t cmd_wel = W25Q64_WriteEnable;            /* 0x06 */
    uint8_t cmd_rsr = W25Q64_Read_Status_Register_1; /* 0x05 */
    uint8_t status = 0;

    for (uint8_t retry = 0; retry < W25Q64_RETRY_MAX; retry++)
    {
        /* 1) 发送写使能命令 */
        if (!W25Q64_SPI_Write(&cmd_wel, 1, W25Qxx_TIMEOUT_VALUE))
        {
            HAL_Delay(W25Q64_RETRY_DELAY_MS);
            continue;
        }

        /* 2) 读状态寄存器1，确认 WEL(bit1) = 1 */
        if (!W25Q64_SPI_WriteThenRead(&cmd_rsr, 1, &status, 1, W25Qxx_TIMEOUT_VALUE))
        {
            HAL_Delay(W25Q64_RETRY_DELAY_MS);
            continue;
        }

        if (status & 0x02)
        {
            return true; /* WEL 已置位，写使能成功 */
        }

        HAL_Delay(W25Q64_RETRY_DELAY_MS);
    }

    return false;
}

/* =========================================================
 *  读状态寄存器1（0x05），也顺手给等待 BUSY 用
 * ========================================================= */
static bool BSP_W25Qxx_ReadStatus(uint8_t *status)
{
    uint8_t cmd = W25Q64_Read_Status_Register_1;

    return W25Q64_SPI_WriteThenRead(&cmd, 1, status, 1, W25Qxx_TIMEOUT_VALUE);
}

/* =========================================================
 *  等待写结束（轮询 BUSY 位），带重试
 * ========================================================= */
static bool BSP_W25Qxx_Wait_for_Write_End(void)
{
    uint8_t state = 0;

    for (uint8_t retry = 0; retry < W25Q64_RETRY_MAX; retry++)
    {
        if (!BSP_W25Qxx_ReadStatus(&state))
        {
            HAL_Delay(W25Q64_RETRY_DELAY_MS);
            continue;
        }

        /* 若 BUSY(bit0) 一直为1，则持续读直到清零 */
        while (state & 0x01)
        {
            if (!BSP_W25Qxx_ReadStatus(&state))
            {
                break; /* 读失败，跳出重试 */
            }
        }

        /* 再次确认 BUSY 已清零 */
        if (!(state & 0x01))
        {
            return true;
        }

        HAL_Delay(W25Q64_RETRY_DELAY_MS);
    }

    return false;
}

/* =========================================================
 *  读取数据（0x03）
 * ========================================================= */
bool BSP_W25Qxx_BufferRead(uint8_t *ReadBuffer, uint32_t ReadAddr, uint16_t NumByteToRead)
{
    uint8_t cmd[4];

    cmd[0] = W25Q64_Read_Data;
    cmd[1] = (uint8_t)(ReadAddr >> 16);
    cmd[2] = (uint8_t)(ReadAddr >> 8);
    cmd[3] = (uint8_t)(ReadAddr);

    for (uint8_t retry = 0; retry < W25Q64_RETRY_MAX; retry++)
    {
        if (W25Q64_SPI_WriteThenRead(cmd, 4,
                                     ReadBuffer, NumByteToRead,
                                     W25Qxx_TIMEOUT_VALUE))
        {
            return true;
        }

        HAL_Delay(W25Q64_RETRY_DELAY_MS);
    }

    return false;
}

/* =========================================================
 *  页写入（0x02，每包 ≤ 256 字节）
 * ========================================================= */
bool BSP_W25Qxx_PageWrite(uint8_t *WriteBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite)
{
    if (NumByteToWrite > W25Q64_PageSize)
    {
        return false;
    }

    uint8_t frame[4 + W25Q64_PageSize];

    frame[0] = W25Q64_Page_Program;
    frame[1] = (uint8_t)(WriteAddr >> 16);
    frame[2] = (uint8_t)(WriteAddr >> 8);
    frame[3] = (uint8_t)(WriteAddr);

    memcpy(&frame[4], WriteBuffer, NumByteToWrite);

    for (uint8_t retry = 0; retry < W25Q64_RETRY_MAX; retry++)
    {
        /* 1) 写使能（内含 WEL 判断） */
        if (!BSP_W25Qxx_WriteEnable())
        {
            HAL_Delay(W25Q64_RETRY_DELAY_MS);
            continue;
        }

        /* 2) 发送命令 + 地址 + 数据 */
        if (!W25Q64_SPI_Write(frame,
                              (uint16_t)(4 + NumByteToWrite),
                              W25Qxx_TIMEOUT_VALUE))
        {
            HAL_Delay(W25Q64_RETRY_DELAY_MS);
            continue;
        }

        /* 3) 等待内部编程完成 */
        if (BSP_W25Qxx_Wait_for_Write_End())
        {
            return true;
        }

        HAL_Delay(W25Q64_RETRY_DELAY_MS);
    }

    return false;
}

/* =========================================================
 *  任意长度写入（自动分页）
 * ========================================================= */
bool BSP_W25Qxx_BufferWrite(uint8_t *WriteBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite)
{
    uint8_t NumOfPage = 0;
    uint8_t NumOfSingle = 0;
    uint8_t Addr = 0;
    uint8_t count = 0;
    uint8_t temp = 0;

    Addr = WriteAddr % W25Q64_PageSize;
    count = W25Q64_PageSize - Addr;
    NumOfPage = NumByteToWrite / W25Q64_PageSize;
    NumOfSingle = NumByteToWrite % W25Q64_PageSize;

    if (Addr == 0)
    {
        if (NumOfPage == 0)
        {
            return BSP_W25Qxx_PageWrite(WriteBuffer, WriteAddr, NumByteToWrite);
        }
        else
        {
            while (NumOfPage--)
            {
                if (!BSP_W25Qxx_PageWrite(WriteBuffer, WriteAddr, W25Q64_PageSize))
                {
                    return false;
                }

                WriteAddr += W25Q64_PageSize;
                WriteBuffer += W25Q64_PageSize;
            }

            if (NumOfSingle)
            {
                return BSP_W25Qxx_PageWrite(WriteBuffer, WriteAddr, NumOfSingle);
            }
        }
    }
    else
    {
        if (NumOfPage == 0)
        {
            if (NumOfSingle > count)
            {
                temp = NumOfSingle - count;

                if (!BSP_W25Qxx_PageWrite(WriteBuffer, WriteAddr, count))
                {
                    return false;
                }

                WriteAddr += count;
                WriteBuffer += count;

                return BSP_W25Qxx_PageWrite(WriteBuffer, WriteAddr, temp);
            }
            else
            {
                return BSP_W25Qxx_PageWrite(WriteBuffer, WriteAddr, NumByteToWrite);
            }
        }
        else
        {
            if (!BSP_W25Qxx_PageWrite(WriteBuffer, WriteAddr, count))
            {
                return false;
            }

            NumByteToWrite -= count;
            NumOfPage = NumByteToWrite / W25Q64_PageSize;
            NumOfSingle = NumByteToWrite % W25Q64_PageSize;

            WriteAddr += count;
            WriteBuffer += count;

            while (NumOfPage--)
            {
                if (!BSP_W25Qxx_PageWrite(WriteBuffer, WriteAddr, W25Q64_PageSize))
                {
                    return false;
                }

                WriteAddr += W25Q64_PageSize;
                WriteBuffer += W25Q64_PageSize;
            }

            if (NumOfSingle)
            {
                return BSP_W25Qxx_PageWrite(WriteBuffer, WriteAddr, NumOfSingle);
            }
        }
    }

    return true;
}

/* =========================================================
 *  扇区擦除（4KB，0x20）
 * ========================================================= */
bool BSP_W25Qxx_SectorErase(uint32_t SectorAddr)
{
    uint8_t cmd[4];

    cmd[0] = W25Q64_Sector_Erase_4KB;
    cmd[1] = (uint8_t)(SectorAddr >> 16);
    cmd[2] = (uint8_t)(SectorAddr >> 8);
    cmd[3] = (uint8_t)(SectorAddr);

    for (uint8_t retry = 0; retry < W25Q64_RETRY_MAX; retry++)
    {
        if (!BSP_W25Qxx_WriteEnable())
        {
            HAL_Delay(W25Q64_RETRY_DELAY_MS);
            continue;
        }

        if (!W25Q64_SPI_Write(cmd, 4, W25Qxx_TIMEOUT_VALUE))
        {
            HAL_Delay(W25Q64_RETRY_DELAY_MS);
            continue;
        }

        if (BSP_W25Qxx_Wait_for_Write_End())
        {
            return true;
        }

        HAL_Delay(W25Q64_RETRY_DELAY_MS);
    }

    return false;
}

/* =========================================================
 *  块擦除（64KB，0xD8）
 * ========================================================= */
bool BSP_W25Qxx_BlockErase(uint32_t BlockAddr)
{
    uint8_t cmd[4];

    cmd[0] = W25Q64_Block_Erase_64KB;
    cmd[1] = (uint8_t)(BlockAddr >> 16);
    cmd[2] = (uint8_t)(BlockAddr >> 8);
    cmd[3] = (uint8_t)(BlockAddr);

    for (uint8_t retry = 0; retry < W25Q64_RETRY_MAX; retry++)
    {
        if (!BSP_W25Qxx_WriteEnable())
        {
            HAL_Delay(W25Q64_RETRY_DELAY_MS);
            continue;
        }

        if (!W25Q64_SPI_Write(cmd, 4, W25Qxx_TIMEOUT_VALUE))
        {
            HAL_Delay(W25Q64_RETRY_DELAY_MS);
            continue;
        }

        if (BSP_W25Qxx_Wait_for_Write_End())
        {
            return true;
        }

        HAL_Delay(W25Q64_RETRY_DELAY_MS);
    }

    return false;
}

/* =========================================================
 *  整片擦除（0xC7，耗时极长，慎用）
 * ========================================================= */
bool BSP_W25Qxx_ChipErase(void)
{
    uint8_t cmd = W25Q64_Chip_Erase;

    for (uint8_t retry = 0; retry < W25Q64_RETRY_MAX; retry++)
    {
        if (!BSP_W25Qxx_WriteEnable())
        {
            HAL_Delay(W25Q64_RETRY_DELAY_MS);
            continue;
        }

        if (!W25Q64_SPI_Write(&cmd, 1, W25Qxx_TIMEOUT_VALUE))
        {
            HAL_Delay(W25Q64_RETRY_DELAY_MS);
            continue;
        }

        if (BSP_W25Qxx_Wait_for_Write_End())
        {
            return true;
        }

        HAL_Delay(W25Q64_RETRY_DELAY_MS);
    }

    return false;
}

/* =========================================================
 *  读取 ID
 * ========================================================= */
bool BSP_W25Qxx_ReadID(uint8_t *ManufacturerID, uint8_t *DeviceID)
{
    uint8_t cmd[4] = {W25Q64_Manufacturer_Device_ID, 0x00, 0x00, 0x00};
    uint8_t buf[2] = {0};

    for (uint8_t retry = 0; retry < W25Q64_RETRY_MAX; retry++)
    {
        if (W25Q64_SPI_WriteThenRead(cmd, 4,
                                     buf, 2,
                                     W25Qxx_TIMEOUT_VALUE))
        {
            *ManufacturerID = buf[0];
            *DeviceID = buf[1];
            return true;
        }

        HAL_Delay(W25Q64_RETRY_DELAY_MS);
    }

    return false;
}