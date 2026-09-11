#ifndef DRIVER_W25Q64_H
#define DRIVER_W25Q64_H

#include <stdint.h>
#include <stdbool.h>

/* ============ 指令宏 ============ */
#define W25Q64_WriteEnable 0x06
#define W25Q64_Read_Status_Register_1 0x05
#define W25Q64_Read_Data 0x03
#define W25Q64_Page_Program 0x02
#define W25Q64_Sector_Erase_4KB 0x20
#define W25Q64_Block_Erase_64KB 0xD8
#define W25Q64_Chip_Erase 0xC7
#define W25Q64_Manufacturer_Device_ID 0x90

/* ============ 参数 ============ */
#define W25Q64_PageSize 256U
#define W25Q64_START_ADDR 0x00000000U
#define W25Q64_SectorSize 4096U
#define W25Q64_BlockSize 65536U
#define W25Q64_ChipSize 8388608U  // 8MB
#define W25Qxx_TIMEOUT_VALUE 100U /* ms */

/* ============ 接口 ============ */
bool BSP_W25Qxx_BufferRead(uint8_t *ReadBuffer, uint32_t ReadAddr, uint16_t NumByteToRead);
bool BSP_W25Qxx_PageWrite(uint8_t *WriteBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite);
bool BSP_W25Qxx_BufferWrite(uint8_t *WriteBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite);
bool BSP_W25Qxx_SectorErase(uint32_t SectorAddr);
bool BSP_W25Qxx_BlockErase(uint32_t BlockAddr);
bool BSP_W25Qxx_ChipErase(void);
bool BSP_W25Qxx_ReadID(uint8_t *ManufacturerID, uint8_t *DeviceID);

#endif /* DRIVER_W25Q64_H */