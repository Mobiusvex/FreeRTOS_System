#include "lcd_driver.h"
#include "main.h"
#include "stdint-gcc.h"
#include "bsp_gpio.h"
#include "bsp_delay.h"
#include "SEGGER_RTT.h"
#include "bsp_lcd_if.h"

// 根据液晶扫描方向而变化的XY像素宽度
// 调用ILI9341_GramScan函数设置方向时会自动更改
uint16_t LCD_X_LENGTH = ILI9341_LESS_PIXEL;
uint16_t LCD_Y_LENGTH = ILI9341_MORE_PIXEL;

// 液晶屏扫描模式，本变量主要用于方便选择触摸屏的计算参数
// 参数可选值为0-7
// 调用ILI9341_GramScan函数设置方向时会自动更改
// LCD刚初始化完成时会使用本默认值
uint8_t lcd_scan_mode = 0;
// 保存液晶屏驱动ic的 ID
static uint16_t lcdid = LCDID_UNKNOWN;

static uint16_t CurrentTextColor = RED;   // 前景色
static uint16_t CurrentBackColor = WHITE; // 背景色

/**
 * @brief  ILI9341背光LED控制
 * @param  enumState ：决定是否使能背光LED
 *   该参数为以下值之一：
 *     @arg ENABLE :使能背光LED
 *     @arg DISABLE :禁用背光LED
 * @retval 无
 */
void ILI9341_BackLed_Control(FunctionalState enumState) {
    if (enumState)
        BSP_GPIO_Write(BSP_GPIO_LCD_BL, BSP_GPIO_LOW);
    else
        BSP_GPIO_Write(BSP_GPIO_LCD_BL, BSP_GPIO_HIGH);
}

/**
 * @brief  用于 ILI9341 简单延时函数
 * @param  nCount ：延时计数值
 * @retval 无
 */
static void ILI9341_Delay(volatile uint32_t nCount) {
    for (; nCount != 0; nCount--);
}

/**
 * @brief  ILI9341 软件复位
 * @param  无
 * @retval 无
 */
void ILI9341_Rst(void) {
    BSP_GPIO_Write(BSP_GPIO_LCD_RST, BSP_GPIO_LOW);
    BSP_DelayMS_Block(1);

    BSP_GPIO_Write(BSP_GPIO_LCD_RST, BSP_GPIO_HIGH);
    BSP_DelayMS_Block(1);
}
/**
 * @brief  读取LCD驱动芯片ID函数，可用于测试底层的读写函数
 * @param  无
 * @retval 正常时返回值为LCD驱动芯片ID: LCDID_ILI9341/LCDID_ST7789V
 *         否则返回: LCDID_UNKNOWN
 */
uint16_t ILI9341_ReadID(void) {
    uint16_t id = 0;

    BSP_LCD_IF_WriteCmd(0x04);
    BSP_LCD_IF_ReadData();
    BSP_LCD_IF_ReadData();
    id = BSP_LCD_IF_ReadData();
    id <<= 8;
    id |= BSP_LCD_IF_ReadData();

    if (id == LCDID_ST7789V) {
        return id;
    } else {
        BSP_LCD_IF_WriteCmd(0xD3);
        BSP_LCD_IF_ReadData();
        BSP_LCD_IF_ReadData();
        id = BSP_LCD_IF_ReadData();
        id <<= 8;
        id |= BSP_LCD_IF_ReadData();
        if (id == LCDID_ILI9341) {
            return id;
        }
    }

    return LCDID_UNKNOWN;
}

/**
 * @brief  初始化ILI9341寄存器
 * @param  无
 * @retval 无
 */
static void ILI9341_REG_Config(void) {
    lcdid = ILI9341_ReadID();
    // SEGGER_RTT_printf(0, "ILI9341_ReadID: %04X\n", lcdid);
    BSP_DelayMS_Block(1);
    if (lcdid == LCDID_ILI9341) {
        /*  Power control B (CFh)  */
        DEBUG_DELAY();
        BSP_LCD_IF_WriteCmd(0xCF);
        BSP_LCD_IF_WriteData(0x00);
        BSP_LCD_IF_WriteData(0x81);
        BSP_LCD_IF_WriteData(0x30);

        /*  Power on sequence control (EDh) */
        DEBUG_DELAY();
        BSP_LCD_IF_WriteCmd(0xED);
        BSP_LCD_IF_WriteData(0x64);
        BSP_LCD_IF_WriteData(0x03);
        BSP_LCD_IF_WriteData(0x12);
        BSP_LCD_IF_WriteData(0x81);

        /*  Driver timing control A (E8h) */
        DEBUG_DELAY();
        BSP_LCD_IF_WriteCmd(0xE8);
        BSP_LCD_IF_WriteData(0x85);
        BSP_LCD_IF_WriteData(0x10);
        BSP_LCD_IF_WriteData(0x78);

        /*  Power control A (CBh) */
        DEBUG_DELAY();
        BSP_LCD_IF_WriteCmd(0xCB);
        BSP_LCD_IF_WriteData(0x39);
        BSP_LCD_IF_WriteData(0x2C);
        BSP_LCD_IF_WriteData(0x00);
        BSP_LCD_IF_WriteData(0x34);
        // BSP_LCD_IF_WriteData ( 0x02 );
        BSP_LCD_IF_WriteData(0x06); // 原来是0x02改为0x06可防止液晶显示白屏时有条纹的情况

        /* Pump ratio control (F7h) */
        DEBUG_DELAY();
        BSP_LCD_IF_WriteCmd(0xF7);
        BSP_LCD_IF_WriteData(0x20);

        /* Driver timing control B */
        DEBUG_DELAY();
        BSP_LCD_IF_WriteCmd(0xEA);
        BSP_LCD_IF_WriteData(0x00);
        BSP_LCD_IF_WriteData(0x00);

        /* Frame Rate Control (In Normal Mode/Full Colors) (B1h) */
        DEBUG_DELAY();
        BSP_LCD_IF_WriteCmd(0xB1);
        BSP_LCD_IF_WriteData(0x00);
        BSP_LCD_IF_WriteData(0x1B);

        /*  Display Function Control (B6h) */
        DEBUG_DELAY();
        BSP_LCD_IF_WriteCmd(0xB6);
        BSP_LCD_IF_WriteData(0x0A);
        BSP_LCD_IF_WriteData(0xA2);

        /* Power Control 1 (C0h) */
        DEBUG_DELAY();
        BSP_LCD_IF_WriteCmd(0xC0);
        BSP_LCD_IF_WriteData(0x35);

        /* Power Control 2 (C1h) */
        DEBUG_DELAY();
        BSP_LCD_IF_WriteCmd(0xC1);
        BSP_LCD_IF_WriteData(0x11);

        /* VCOM Control 1 (C5h) */
        BSP_LCD_IF_WriteCmd(0xC5);
        BSP_LCD_IF_WriteData(0x45);
        BSP_LCD_IF_WriteData(0x45);

        /*  VCOM Control 2 (C7h)  */
        BSP_LCD_IF_WriteCmd(0xC7);
        BSP_LCD_IF_WriteData(0xA2);

        /* Enable 3G (F2h) */
        BSP_LCD_IF_WriteCmd(0xF2);
        BSP_LCD_IF_WriteData(0x00);

        /* Gamma Set (26h) */
        BSP_LCD_IF_WriteCmd(0x26);
        BSP_LCD_IF_WriteData(0x01);
        DEBUG_DELAY();

        /* Positive Gamma Correction */
        BSP_LCD_IF_WriteCmd(0xE0); // Set Gamma
        BSP_LCD_IF_WriteData(0x0F);
        BSP_LCD_IF_WriteData(0x26);
        BSP_LCD_IF_WriteData(0x24);
        BSP_LCD_IF_WriteData(0x0B);
        BSP_LCD_IF_WriteData(0x0E);
        BSP_LCD_IF_WriteData(0x09);
        BSP_LCD_IF_WriteData(0x54);
        BSP_LCD_IF_WriteData(0xA8);
        BSP_LCD_IF_WriteData(0x46);
        BSP_LCD_IF_WriteData(0x0C);
        BSP_LCD_IF_WriteData(0x17);
        BSP_LCD_IF_WriteData(0x09);
        BSP_LCD_IF_WriteData(0x0F);
        BSP_LCD_IF_WriteData(0x07);
        BSP_LCD_IF_WriteData(0x00);

        /* Negative Gamma Correction (E1h) */
        BSP_LCD_IF_WriteCmd(0XE1); // Set Gamma
        BSP_LCD_IF_WriteData(0x00);
        BSP_LCD_IF_WriteData(0x19);
        BSP_LCD_IF_WriteData(0x1B);
        BSP_LCD_IF_WriteData(0x04);
        BSP_LCD_IF_WriteData(0x10);
        BSP_LCD_IF_WriteData(0x07);
        BSP_LCD_IF_WriteData(0x2A);
        BSP_LCD_IF_WriteData(0x47);
        BSP_LCD_IF_WriteData(0x39);
        BSP_LCD_IF_WriteData(0x03);
        BSP_LCD_IF_WriteData(0x06);
        BSP_LCD_IF_WriteData(0x06);
        BSP_LCD_IF_WriteData(0x30);
        BSP_LCD_IF_WriteData(0x38);
        BSP_LCD_IF_WriteData(0x0F);

        /* memory access control set */
        DEBUG_DELAY();
        BSP_LCD_IF_WriteCmd(0x36);
        BSP_LCD_IF_WriteData(0xC8); /*竖屏  左上角到 (起点)到右下角 (终点)扫描方式*/
        DEBUG_DELAY();

        /* column address control set */
        BSP_LCD_IF_WriteCmd(CMD_SetCoordinateX);
        BSP_LCD_IF_WriteData(0x00);
        BSP_LCD_IF_WriteData(0x00);
        BSP_LCD_IF_WriteData(0x00);
        BSP_LCD_IF_WriteData(0xEF);

        /* page address control set */
        DEBUG_DELAY();
        BSP_LCD_IF_WriteCmd(CMD_SetCoordinateY);
        BSP_LCD_IF_WriteData(0x00);
        BSP_LCD_IF_WriteData(0x00);
        BSP_LCD_IF_WriteData(0x01);
        BSP_LCD_IF_WriteData(0x3F);

        /*  Pixel Format Set (3Ah)  */
        DEBUG_DELAY();
        BSP_LCD_IF_WriteCmd(0x3a);
        BSP_LCD_IF_WriteData(0x55);

        /* Sleep Out (11h)  */
        BSP_LCD_IF_WriteCmd(0x11);
        BSP_DelayMS_Block(10);
        DEBUG_DELAY();

        /* Display ON (29h) */
        BSP_LCD_IF_WriteCmd(0x29);
    }

    else if (lcdid == LCDID_ST7789V) {
        /*  Power control B (CFh)  */
        DEBUG_DELAY();
        BSP_LCD_IF_WriteCmd(0xCF);
        BSP_LCD_IF_WriteData(0x00);
        BSP_LCD_IF_WriteData(0xC1);
        BSP_LCD_IF_WriteData(0x30);

        /*  Power on sequence control (EDh) */
        DEBUG_DELAY();
        BSP_LCD_IF_WriteCmd(0xED);
        BSP_LCD_IF_WriteData(0x64);
        BSP_LCD_IF_WriteData(0x03);
        BSP_LCD_IF_WriteData(0x12);
        BSP_LCD_IF_WriteData(0x81);

        /*  Driver timing control A (E8h) */
        DEBUG_DELAY();
        BSP_LCD_IF_WriteCmd(0xE8);
        BSP_LCD_IF_WriteData(0x85);
        BSP_LCD_IF_WriteData(0x10);
        BSP_LCD_IF_WriteData(0x78);

        /*  Power control A (CBh) */
        DEBUG_DELAY();
        BSP_LCD_IF_WriteCmd(0xCB);
        BSP_LCD_IF_WriteData(0x39);
        BSP_LCD_IF_WriteData(0x2C);
        BSP_LCD_IF_WriteData(0x00);
        BSP_LCD_IF_WriteData(0x34);
        BSP_LCD_IF_WriteData(0x02);

        /* Pump ratio control (F7h) */
        DEBUG_DELAY();
        BSP_LCD_IF_WriteCmd(0xF7);
        BSP_LCD_IF_WriteData(0x20);

        /* Driver timing control B */
        DEBUG_DELAY();
        BSP_LCD_IF_WriteCmd(0xEA);
        BSP_LCD_IF_WriteData(0x00);
        BSP_LCD_IF_WriteData(0x00);

        /* Power Control 1 (C0h) */
        DEBUG_DELAY();
        BSP_LCD_IF_WriteCmd(0xC0);  // Power control
        BSP_LCD_IF_WriteData(0x21); // VRH[5:0]

        /* Power Control 2 (C1h) */
        DEBUG_DELAY();
        BSP_LCD_IF_WriteCmd(0xC1);  // Power control
        BSP_LCD_IF_WriteData(0x11); // SAP[2:0];BT[3:0]

        /* VCOM Control 1 (C5h) */
        BSP_LCD_IF_WriteCmd(0xC5);
        BSP_LCD_IF_WriteData(0x2D);
        BSP_LCD_IF_WriteData(0x33);

        /*  VCOM Control 2 (C7h)  */
        //	BSP_LCD_IF_WriteCmd ( 0xC7 );
        //	BSP_LCD_IF_WriteData ( 0XC0 );

        /* memory access control set */
        DEBUG_DELAY();
        BSP_LCD_IF_WriteCmd(0x36);  // Memory Access Control
        BSP_LCD_IF_WriteData(0x00); /*竖屏  左上角到 (起点)到右下角 (终点)扫描方式*/
        DEBUG_DELAY();

        BSP_LCD_IF_WriteCmd(0x3A);
        BSP_LCD_IF_WriteData(0x55);

        /* Frame Rate Control (In Normal Mode/Full Colors) (B1h) */
        DEBUG_DELAY();
        BSP_LCD_IF_WriteCmd(0xB1);
        BSP_LCD_IF_WriteData(0x00);
        BSP_LCD_IF_WriteData(0x17);

        /*  Display Function Control (B6h) */
        DEBUG_DELAY();
        BSP_LCD_IF_WriteCmd(0xB6);
        BSP_LCD_IF_WriteData(0x0A);
        BSP_LCD_IF_WriteData(0xA2);

        BSP_LCD_IF_WriteCmd(0xF6);
        BSP_LCD_IF_WriteData(0x01);
        BSP_LCD_IF_WriteData(0x30);

        /* Enable 3G (F2h) */
        BSP_LCD_IF_WriteCmd(0xF2);
        BSP_LCD_IF_WriteData(0x00);

        /* Gamma Set (26h) */
        BSP_LCD_IF_WriteCmd(0x26);
        BSP_LCD_IF_WriteData(0x01);
        DEBUG_DELAY();

        /* Positive Gamma Correction */
        BSP_LCD_IF_WriteCmd(0xe0); // Positive gamma
        BSP_LCD_IF_WriteData(0xd0);
        BSP_LCD_IF_WriteData(0x00);
        BSP_LCD_IF_WriteData(0x02);
        BSP_LCD_IF_WriteData(0x07);
        BSP_LCD_IF_WriteData(0x0b);
        BSP_LCD_IF_WriteData(0x1a);
        BSP_LCD_IF_WriteData(0x31);
        BSP_LCD_IF_WriteData(0x54);
        BSP_LCD_IF_WriteData(0x40);
        BSP_LCD_IF_WriteData(0x29);
        BSP_LCD_IF_WriteData(0x12);
        BSP_LCD_IF_WriteData(0x12);
        BSP_LCD_IF_WriteData(0x12);
        BSP_LCD_IF_WriteData(0x17);

        /* Negative Gamma Correction (E1h) */
        BSP_LCD_IF_WriteCmd(0xe1); // Negative gamma
        BSP_LCD_IF_WriteData(0xd0);
        BSP_LCD_IF_WriteData(0x00);
        BSP_LCD_IF_WriteData(0x02);
        BSP_LCD_IF_WriteData(0x07);
        BSP_LCD_IF_WriteData(0x05);
        BSP_LCD_IF_WriteData(0x25);
        BSP_LCD_IF_WriteData(0x2d);
        BSP_LCD_IF_WriteData(0x44);
        BSP_LCD_IF_WriteData(0x45);
        BSP_LCD_IF_WriteData(0x1c);
        BSP_LCD_IF_WriteData(0x18);
        BSP_LCD_IF_WriteData(0x16);
        BSP_LCD_IF_WriteData(0x1c);
        BSP_LCD_IF_WriteData(0x1d);

        /* Sleep Out (11h)  */
        BSP_LCD_IF_WriteCmd(0x11); // Exit Sleep
        BSP_DelayMS_Block(100);
        DEBUG_DELAY();

        /* Display ON (29h) */
        BSP_LCD_IF_WriteCmd(0x29); // Display on

        BSP_LCD_IF_WriteCmd(0x2c);
    }
}

/**
 * @brief  设置ILI9341的GRAM的扫描方向
 * @param  ucOption ：选择GRAM的扫描方向
 *     @arg 0-7 :参数可选值为0-7这八个方向
 *
 *	！！！其中0、3、5、6 模式适合从左至右显示文字，
 *				不推荐使用其它模式显示文字	其它模式显示文字会有镜像效果
 *
 *	其中0、2、4、6 模式的X方向像素为240，Y方向像素为320
 *	其中1、3、5、7 模式下X方向像素为320，Y方向像素为240
 *
 *	其中 6 模式为大部分液晶例程的默认显示方向
 *	其中 3 模式为摄像头例程使用的方向
 *	其中 0 模式为BMP图片显示例程使用的方向
 *
 * @retval 无
 * @note  坐标图例：A表示向上，V表示向下，<表示向左，>表示向右
                    X表示X轴，Y表示Y轴

                                             LCDID_ILI9341
------------------------------------------------------------
模式0：				.		模式1：		.	模式2：			.	模式3：
                    A		.					A		.		A					.		A
                    |		.					|		.		|					.		|
                    Y		.					X		.		Y					.		X
                    0		.					1		.		2					.		3
    <--- X0 o		.	<----Y1	o		.		o 2X--->  .		o 3Y--->
------------------------------------------------------------
模式4：				.	模式5：			.	模式6：			.	模式7：
    <--- X4 o		.	<--- Y5 o		.		o 6X--->  .		o 7Y--->
                    4		.					5		.		6					.		7
                    Y		.					X		.		Y					.		X
                    |		.					|		.		|					.		|
                    V		.					V		.		V					.		V
---------------------------------------------------------
                                             LCD屏示例
                                |-----------------|
                                |			野火Logo		|
                                |									|
                                |									|
                                |									|
                                |									|
                                |									|
                                |									|
                                |									|
                                |									|
                                |-----------------|
                                屏幕正面（宽240，高320）




                                      LCDID_ST7789V
------------------------------------------------------------
模式0：				.		模式1：		.	模式2：			.	模式3：
    o 0X--->  	.		o 1Y--->  .	<--- X2 o		.	<--- Y3 o
    0						.		1					.					2		.					3
    Y						.		X					.					Y		.					X
    |						.		|					.					|		.					|
    V								V					.					V		.					V
------------------------------------------------------------
模式4：				.	模式5：			.	模式6：		.	模式7：
    A						.		A					.					A	.					A
    |						.		|					.					|	.					|
    Y						.		X					.					Y	.					X
    4						.		5					.					6	.					7
    o 4X--->  	.		o 5Y--->  .	<--- X6 o	.	<--- Y7 o
---------------------------------------------------------
                                             LCD屏示例
                                |-----------------|
                                |			野火Logo		|
                                |									|
                                |									|
                                |									|
                                |									|
                                |									|
                                |									|
                                |									|
                                |									|
                                |-----------------|
                                屏幕正面（宽240，高320）

 *******************************************************/
void ILI9341_GramScan(uint8_t ucOption) {
    // 参数检查，只可输入0-7
    if (ucOption > 7)
        return;

    // 根据模式更新lcd_scan_mode的值，主要用于触摸屏选择计算参数
    lcd_scan_mode = ucOption;

    // 根据模式更新XY方向的像素宽度
    if (ucOption % 2 == 0) {
        // 0 2 4 6模式下X方向像素宽度为240，Y方向为320
        LCD_X_LENGTH = ILI9341_LESS_PIXEL;
        LCD_Y_LENGTH = ILI9341_MORE_PIXEL;
    } else {
        // 1 3 5 7模式下X方向像素宽度为320，Y方向为240
        LCD_X_LENGTH = ILI9341_MORE_PIXEL;
        LCD_Y_LENGTH = ILI9341_LESS_PIXEL;
    }

    // 0x36命令参数的高3位可用于设置GRAM扫描方向
    BSP_LCD_IF_WriteCmd(0x36);
    if (lcdid == LCDID_ILI9341) {
        BSP_LCD_IF_WriteData(0x08 | (ucOption << 5)); // 根据ucOption的值设置LCD参数，共0-7种模式
    } else if (lcdid == LCDID_ST7789V) {
        BSP_LCD_IF_WriteData(0x00 | (ucOption << 5)); // 根据ucOption的值设置LCD参数，共0-7种模式
    }
    BSP_LCD_IF_WriteCmd(CMD_SetCoordinateX);
    BSP_LCD_IF_WriteData(0x00);                             /* x 起始坐标高8位 */
    BSP_LCD_IF_WriteData(0x00);                             /* x 起始坐标低8位 */
    BSP_LCD_IF_WriteData(((LCD_X_LENGTH - 1) >> 8) & 0xFF); /* x 结束坐标高8位 */
    BSP_LCD_IF_WriteData((LCD_X_LENGTH - 1) & 0xFF);        /* x 结束坐标低8位 */

    BSP_LCD_IF_WriteCmd(CMD_SetCoordinateY);
    BSP_LCD_IF_WriteData(0x00);                             /* y 起始坐标高8位 */
    BSP_LCD_IF_WriteData(0x00);                             /* y 起始坐标低8位 */
    BSP_LCD_IF_WriteData(((LCD_Y_LENGTH - 1) >> 8) & 0xFF); /* y 结束坐标高8位 */
    BSP_LCD_IF_WriteData((LCD_Y_LENGTH - 1) & 0xFF);        /* y 结束坐标低8位 */

    /* write gram start */
    BSP_LCD_IF_WriteCmd(CMD_SetPixel);
}

/**
 * @brief  ILI9341初始化函数，如果要用到lcd，一定要调用这个函数
 * @param  无
 * @retval 无
 */
void ILI9341_Init(void) {
    ILI9341_BackLed_Control(ENABLE); // 点亮LCD背光灯
    ILI9341_Rst();
    ILI9341_REG_Config();

    // 设置默认扫描方向，其中 6 模式为大部分液晶例程的默认显示方向
    ILI9341_GramScan(lcd_scan_mode);
}

/**
 * @brief  在ILI9341显示器上开辟一个窗口
 * @param  usX ：在特定扫描方向下窗口的起点X坐标
 * @param  usY ：在特定扫描方向下窗口的起点Y坐标
 * @param  usWidth ：窗口的宽度
 * @param  usHeight ：窗口的高度
 * @retval 无
 */
void ILI9341_OpenWindow(uint16_t usX, uint16_t usY, uint16_t usWidth, uint16_t usHeight) {
    BSP_LCD_IF_WriteCmd(CMD_SetCoordinateX); /* 设置X坐标 */
    BSP_LCD_IF_WriteData(usX >> 8);          /* 先高8位，然后低8位 */
    BSP_LCD_IF_WriteData(usX & 0xff);        /* 设置起始点和结束点*/
    BSP_LCD_IF_WriteData((usX + usWidth - 1) >> 8);
    BSP_LCD_IF_WriteData((usX + usWidth - 1) & 0xff);

    BSP_LCD_IF_WriteCmd(CMD_SetCoordinateY); /* 设置Y坐标*/
    BSP_LCD_IF_WriteData(usY >> 8);
    BSP_LCD_IF_WriteData(usY & 0xff);
    BSP_LCD_IF_WriteData((usY + usHeight - 1) >> 8);
    BSP_LCD_IF_WriteData((usY + usHeight - 1) & 0xff);
}

/**
 * @brief  设定ILI9341的光标坐标
 * @param  usX ：在特定扫描方向下光标的X坐标
 * @param  usY ：在特定扫描方向下光标的Y坐标
 * @retval 无
 */
static void ILI9341_SetCursor(uint16_t usX, uint16_t usY) {
    ILI9341_OpenWindow(usX, usY, 1, 1);
}

/**
 * @brief  在ILI9341显示器上以某一颜色填充像素点
 * @param  ulAmout_Point ：要填充颜色的像素点的总数目
 * @param  usColor ：颜色
 * @retval 无
 */
static __inline void ILI9341_FillColor(uint32_t ulAmout_Point, uint16_t usColor) {
    uint32_t i = 0;

    /* memory write */
    BSP_LCD_IF_WriteCmd(CMD_SetPixel);

    for (i = 0; i < ulAmout_Point; i++)
        BSP_LCD_IF_WriteData(usColor);
}

/**
 * @brief  对ILI9341显示器的某一窗口以某种颜色进行清屏
 * @param  usX ：在特定扫描方向下窗口的起点X坐标
 * @param  usY ：在特定扫描方向下窗口的起点Y坐标
 * @param  usWidth ：窗口的宽度
 * @param  usHeight ：窗口的高度
 * @note 可使用LCD_SetBackColor、LCD_SetTextColor、LCD_SetColors函数设置颜色
 * @retval 无
 */
void ILI9341_Clear(uint16_t usX, uint16_t usY, uint16_t usWidth, uint16_t usHeight) {
    ILI9341_OpenWindow(usX, usY, usWidth, usHeight);

    ILI9341_FillColor(usWidth * usHeight, CurrentBackColor);
}

/**
 * @brief  设置LCD的前景(字体)颜色,RGB565
 * @param  Color: 指定前景(字体)颜色
 * @retval None
 */
void LCD_SetTextColor(uint16_t Color) {
    CurrentTextColor = Color;
}

/**
 * @brief  设置LCD的背景颜色,RGB565
 * @param  Color: 指定背景颜色
 * @retval None
 */
void LCD_SetBackColor(uint16_t Color) {
    CurrentBackColor = Color;
}

/**
 * @brief  对ILI9341显示器的某一点以某种颜色进行填充
 * @param  usX ：在特定扫描方向下该点的X坐标
 * @param  usY ：在特定扫描方向下该点的Y坐标
 * @note 可使用LCD_SetBackColor、LCD_SetTextColor、LCD_SetColors函数设置颜色
 * @retval 无
 */
void ILI9341_SetPointPixel(uint16_t usX, uint16_t usY) {
    if ((usX < LCD_X_LENGTH) && (usY < LCD_Y_LENGTH)) {
        ILI9341_SetCursor(usX, usY);

        ILI9341_FillColor(1, CurrentTextColor);
    }
}

/**
 * @brief  读取 GRAM 的一个像素数据
 * @param  无
 * @retval 像素数据
 */
static uint16_t ILI9341_Read_PixelData(void) {
    uint16_t usRG = 0, usB = 0;

    BSP_LCD_IF_WriteCmd(0x2E); /* 读数据 */
    // 去掉前一次读取结果
    BSP_LCD_IF_ReadData(); /*FIRST READ OUT DUMMY DATA*/

    // 获取红色通道与绿色通道的值
    usRG = BSP_LCD_IF_ReadData(); /*READ OUT RED AND GREEN DATA  */
    usB = BSP_LCD_IF_ReadData();  /*READ OUT BLUE DATA*/

    return ((usRG & 0xF800) | ((usRG << 3) & 0x7E0) | (usB >> 11));
}

/**
 * @brief  获取 ILI9341 显示器上某一个坐标点的像素数据
 * @param  usX ：在特定扫描方向下该点的X坐标
 * @param  usY ：在特定扫描方向下该点的Y坐标
 * @retval 像素数据
 */
uint16_t ILI9341_GetPointPixel(uint16_t usX, uint16_t usY) {
    uint16_t usPixelData;

    ILI9341_SetCursor(usX, usY);

    usPixelData = ILI9341_Read_PixelData();

    return usPixelData;
}

/**
 * @brief  在 ILI9341 显示器上使用 Bresenham 算法画线段
 * @param  usX1 ：在特定扫描方向下线段的一个端点X坐标
 * @param  usY1 ：在特定扫描方向下线段的一个端点Y坐标
 * @param  usX2 ：在特定扫描方向下线段的另一个端点X坐标
 * @param  usY2 ：在特定扫描方向下线段的另一个端点Y坐标
 * @note 可使用LCD_SetBackColor、LCD_SetTextColor、LCD_SetColors函数设置颜色
 * @retval 无
 */
void ILI9341_DrawLine(uint16_t usX1, uint16_t usY1, uint16_t usX2, uint16_t usY2) {
    uint16_t us;
    uint16_t usX_Current, usY_Current;

    int32_t lError_X = 0, lError_Y = 0, lDelta_X, lDelta_Y, lDistance;
    int32_t lIncrease_X, lIncrease_Y;

    lDelta_X = usX2 - usX1; // 计算坐标增量
    lDelta_Y = usY2 - usY1;

    usX_Current = usX1;
    usY_Current = usY1;

    if (lDelta_X > 0)
        lIncrease_X = 1; // 设置单步方向

    else if (lDelta_X == 0)
        lIncrease_X = 0; // 垂直线

    else {
        lIncrease_X = -1;
        lDelta_X = -lDelta_X;
    }

    if (lDelta_Y > 0)
        lIncrease_Y = 1;

    else if (lDelta_Y == 0)
        lIncrease_Y = 0; // 水平线

    else {
        lIncrease_Y = -1;
        lDelta_Y = -lDelta_Y;
    }

    if (lDelta_X > lDelta_Y)
        lDistance = lDelta_X; // 选取基本增量坐标轴

    else
        lDistance = lDelta_Y;

    for (us = 0; us <= lDistance + 1; us++) // 画线输出
    {
        ILI9341_SetPointPixel(usX_Current, usY_Current); // 画点

        lError_X += lDelta_X;
        lError_Y += lDelta_Y;

        if (lError_X > lDistance) {
            lError_X -= lDistance;
            usX_Current += lIncrease_X;
        }

        if (lError_Y > lDistance) {
            lError_Y -= lDistance;
            usY_Current += lIncrease_Y;
        }
    }
}

/**
 * @brief  在 ILI9341 显示器上画一个矩形
 * @param  usX_Start ：在特定扫描方向下矩形的起始点X坐标
 * @param  usY_Start ：在特定扫描方向下矩形的起始点Y坐标
 * @param  usWidth：矩形的宽度（单位：像素）
 * @param  usHeight：矩形的高度（单位：像素）
 * @param  ucFilled ：选择是否填充该矩形
 *   该参数为以下值之一：
 *     @arg 0 :空心矩形
 *     @arg 1 :实心矩形
 * @note 可使用LCD_SetBackColor、LCD_SetTextColor、LCD_SetColors函数设置颜色
 * @retval 无
 */
void ILI9341_DrawRectangle(uint16_t usX_Start, uint16_t usY_Start, uint16_t usWidth, uint16_t usHeight, uint8_t ucFilled) {
    if (ucFilled) {
        ILI9341_OpenWindow(usX_Start, usY_Start, usWidth, usHeight);
        ILI9341_FillColor(usWidth * usHeight, CurrentTextColor);
    } else {
        ILI9341_DrawLine(usX_Start, usY_Start, usX_Start + usWidth - 1, usY_Start);
        ILI9341_DrawLine(usX_Start, usY_Start + usHeight - 1, usX_Start + usWidth - 1, usY_Start + usHeight - 1);
        ILI9341_DrawLine(usX_Start, usY_Start, usX_Start, usY_Start + usHeight - 1);
        ILI9341_DrawLine(usX_Start + usWidth - 1, usY_Start, usX_Start + usWidth - 1, usY_Start + usHeight - 1);
    }
}

/**
 * @brief  ÉèÖÃLCDµÄÇ°¾°(×ÖÌå)¼°±³¾°ÑÕÉ«,RGB565
 * @param  TextColor: Ö¸¶¨Ç°¾°(×ÖÌå)ÑÕÉ«
 * @param  BackColor: Ö¸¶¨±³¾°ÑÕÉ«
 * @retval None
 */
void LCD_SetColors(uint16_t TextColor, uint16_t BackColor) {
    CurrentTextColor = TextColor;
    CurrentBackColor = BackColor;
}

/**
 * @brief  显示器的某一点以某种颜色进行填充
 * @param  usX ：在特定扫描方向下该点的X坐标
 * @param  usY ：在特定扫描方向下该点的Y坐标
 * @param  color ：填充的颜色
 * @retval 无
 */
void lcd_draw_point(uint16_t usX, uint16_t usY, uint16_t color) {
    if ((usX < LCD_X_LENGTH) && (usY < LCD_Y_LENGTH)) {
        ILI9341_SetCursor(usX, usY);
        ILI9341_FillColor(1, color);
    }
}