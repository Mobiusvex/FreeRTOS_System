#include "flash_driver.h"



void FLASH_ERASE_PAGE(uint32_t FLASH_Addr)
{
    
	HAL_FLASH_Unlock();

	FLASH_EraseInitTypeDef FLASH_Init;
	FLASH_Init.TypeErase = FLASH_TYPEERASE_PAGES;
	FLASH_Init.PageAddress = FLASH_Addr;
	FLASH_Init.NbPages = 1;
	
	uint32_t PageError = 0;
	
	HAL_FLASHEx_Erase(&FLASH_Init,&PageError);


	HAL_FLASH_Lock();
}
/*从指定地址开始写入数据*/
//FLASH_Addr:起始地址
//FLASH_Data:写入数据指针(写入的数据为16位，至少都要16位)
//Size:写入数据长度

void FLASH_WriteData(uint32_t FLASH_Addr, uint32_t *FLASH_Data, uint16_t Size)
{

	HAL_FLASH_Unlock();
	
	uint32_t TempBuf = 0;
	for(uint16_t i=0;i<Size;i++)
	{
		TempBuf = *(FLASH_Data+i);
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,(FLASH_Addr + (i*4)),TempBuf);
	}
	
	HAL_FLASH_Lock();
}

//从指定地址读出数据
//faddr：地址
uint32_t FLASH_ReadWord(uint32_t faddr)
{
	return *(__IO uint32_t*)faddr;
}

//从指定地址开始读出指定长度的数据
//ReadAddr:起始指针
//pBuffer:数据指针
//NumToRead:大小（至少半字(16位)数）
void FLASH_ReadData(uint32_t ReadAddr,uint32_t *pBuffer,uint16_t NumToRead)
{
	for(uint16_t i=0;i<NumToRead;i++)
	{
		pBuffer[i] = FLASH_ReadWord(ReadAddr);
		ReadAddr+=4;
	}
}

