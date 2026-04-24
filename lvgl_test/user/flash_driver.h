#ifndef __FLASH_DRIVER_H
#define __FLASH_DRIVER_H

#include "main.h"
#include "string.h"
#include "stdint-gcc.h"

#define       STM32_FLASH_SIZE 512   //所选STM32的FLASH容量大小(单位为K)；本产品选用的型号为：STM32F103C8T6；FALSH大小为64K

     #if      STM32_FLASH_SIZE < 256
		 #define  STM32_SECTOR_SIZE 1024  //一页为1K
		 #else
		 #define  STM32_SECTOR_SIZE 2048   //一页为2K
		 #endif
		 
#define STM32_FLASH_BASE 0x08000000 //STM32 FLASH起始地址

//写入的FLASH地址，这里为从倒数第一个扇区地址(0x807f800)开始写
//STM32_FLASH_BASE + STM32_SECTOR_SIZE*255 = 0x08000000 + (2048*255) = 0x0807f800
#define FLASH_SAVE_ADDR  STM32_FLASH_BASE+STM32_SECTOR_SIZE*255
		 
void FLASH_ERASE_PAGE(uint32_t FLASH_Addr);
void FLASH_WriteData(uint32_t FLASH_Addr, uint32_t *FLASH_Data, uint16_t Size);
uint32_t FLASH_ReadWord(uint32_t faddr);
void FLASH_ReadData(uint32_t ReadAddr,uint32_t *pBuffer,uint16_t NumToRead);


#endif /* __FLASH_DRIVER_H */
