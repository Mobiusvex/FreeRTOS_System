#ifndef __BSP_LCD_IF_H__
#define __BSP_LCD_IF_H__

#include <stdint.h>

// 核心传输接口：写命令、写数据、批量写像素
void BSP_LCD_IF_WriteCmd(uint16_t cmd);
void BSP_LCD_IF_WriteData(uint16_t data);
uint16_t BSP_LCD_IF_ReadData(void);

#endif
