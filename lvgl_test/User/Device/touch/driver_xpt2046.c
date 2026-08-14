#include "driver_xpt2046.h"
#include "stdint-gcc.h"
#include "bsp_spi.h"
#include "driver_lcd.h"
#include "bsp_delay.h"
#include "bsp_flash.h"

/******************************* 声明 XPT2046 相关的静态函数 ***************************/
static uint16_t XPT2046_ReadAdc(uint8_t ucChannel);
static void XPT2046_ReadAdc_XY(int16_t *sX_Ad, int16_t *sY_Ad);
static uint8_t XPT2046_ReadAdc_Smooth_XY(strType_XPT2046_Coordinate *pScreenCoordinate);
static uint8_t XPT2046_Calculate_CalibrationFactor(strType_XPT2046_Coordinate *pDisplayCoordinate, strType_XPT2046_Coordinate *pScreenSample, strType_XPT2046_Calibration *pCalibrationFactor);
static void ILI9341_DrawCross(uint16_t usX, uint16_t usY);

/******************************* 定义 XPT2046 全局变量 ***************************/
// 默认触摸参数，不同的屏幕稍有差异，可重新调用触摸校准函数获取
strType_XPT2046_TouchPara strXPT2046_TouchPara[8] = {
    {0, -0.006464, -0.073259, 280.358032, 0.074878, 0.002052, -6.545977},   // 扫描方式0
    {0, 0.086314, 0.001891, -12.836658, -0.003722, -0.065799, 254.715714},  // 扫描方式1
    {0, 0.002782, 0.061522, -11.595689, 0.083393, 0.005159, -15.650089},    // 扫描方式2
    {0, 0.089743, -0.000289, -20.612209, -0.001374, 0.064451, -16.054003},  // 扫描方式3
    {0, 0.000767, -0.068258, 250.891769, -0.085559, -0.000195, 334.747650}, // 扫描方式4
    {0, -0.084744, 0.000047, 323.163147, -0.002109, -0.066371, 260.985809}, // 扫描方式5
    {0, -0.001848, 0.066984, -12.807136, -0.084858, -0.000805, 333.395386}, // 扫描方式6
    {0, -0.085470, -0.000876, 334.023163, -0.003390, 0.064725, -6.211169}   // 扫描方式7
};

volatile uint8_t ucXPT2046_TouchFlag = 0;

/**
 * @brief  对 XPT2046 选择一个模拟通道后，启动ADC，并返回ADC采样结果
 * @param  ucChannel
 *   该参数为以下值之一：
 *     @arg 0x90 :通道Y+的选择控制字
 *     @arg 0xd0 :通道X+的选择控制字
 * @retval 该通道的ADC采样结果
 */
static uint16_t XPT2046_ReadAdc(uint8_t ucChannel) {
    uint8_t tx_data[3] = {ucChannel, 0x00, 0X00};
    uint8_t rx_data[3];
    uint16_t res = 0;
#if SPI_TEST
    // TODO:时序好像有问题，会导致触摸屏失效
    BSP_SPI_TransmitReceive(BSP_SPI_BUS_XPT2046, tx_data, &res, 1, 0);
#else
    BSP_SPI_TransmitReceive(BSP_SPI_BUS_XPT2046, tx_data, rx_data, 3, 0);
    res = (uint16_t)(rx_data[1] << 4 | rx_data[2] >> 4);
#endif
    return res;
}

/**
 * @brief  读取 XPT2046 的X通道和Y通道的AD值（12 bit，最大是4096）
 * @param  sX_Ad ：存放X通道AD值的地址
 * @param  sY_Ad ：存放Y通道AD值的地址
 * @retval 无
 */
static void XPT2046_ReadAdc_XY(int16_t *sX_Ad, int16_t *sY_Ad) {
    int16_t sX_Ad_Temp, sY_Ad_Temp;

    sX_Ad_Temp = XPT2046_ReadAdc(XPT2046_CHANNEL_X);

    BSP_DelayUS(1);

    sY_Ad_Temp = XPT2046_ReadAdc(XPT2046_CHANNEL_Y);

    *sX_Ad = sX_Ad_Temp;
    *sY_Ad = sY_Ad_Temp;
}

/**
 * @brief  在触摸 XPT2046 屏幕时获取一组坐标的AD值，并对该坐标进行滤波
 * @param  无
 * @retval 滤波之后的坐标AD值
 */
static uint8_t XPT2046_ReadAdc_Smooth_XY(strType_XPT2046_Coordinate *pScreenCoordinate) {
    uint8_t ucCount = 0, i;

    int16_t sAD_X, sAD_Y;
    int16_t sBufferArray[2][10] = {{0}, {0}}; // 坐标X和Y进行多次采样

    // 存储采样中的最小值、最大值
    int32_t lX_Min, lX_Max, lY_Min, lY_Max;

    /* 循环采样10次 */
    do {
        XPT2046_ReadAdc_XY(&sAD_X, &sAD_Y);

        sBufferArray[0][ucCount] = sAD_X;
        sBufferArray[1][ucCount] = sAD_Y;

        ucCount++;

    } while ((XPT2046_PENIRQ_Read() == XPT2046_PENIRQ_ActiveLevel) && (ucCount < 10)); // 用户点击触摸屏时即TP_INT_IN信号为低 并且 ucCount<10

    /*如果触笔弹起*/
    if (XPT2046_PENIRQ_Read() != XPT2046_PENIRQ_ActiveLevel)
        ucXPT2046_TouchFlag = 0; // 中断标志复位

    /*如果成功采样10个样本*/
    if (ucCount == 10) {
        lX_Max = lX_Min = sBufferArray[0][0];
        lY_Max = lY_Min = sBufferArray[1][0];

        for (i = 1; i < 10; i++) {
            if (sBufferArray[0][i] < lX_Min)
                lX_Min = sBufferArray[0][i];

            else if (sBufferArray[0][i] > lX_Max)
                lX_Max = sBufferArray[0][i];
        }

        for (i = 1; i < 10; i++) {
            if (sBufferArray[1][i] < lY_Min)
                lY_Min = sBufferArray[1][i];

            else if (sBufferArray[1][i] > lY_Max)
                lY_Max = sBufferArray[1][i];
        }

        /*去除最小值和最大值之后求平均值*/
        pScreenCoordinate->x = (sBufferArray[0][0] + sBufferArray[0][1] + sBufferArray[0][2] + sBufferArray[0][3] + sBufferArray[0][4] + sBufferArray[0][5] + sBufferArray[0][6] + sBufferArray[0][7] + sBufferArray[0][8] + sBufferArray[0][9] - lX_Min - lX_Max) >> 3;

        pScreenCoordinate->y = (sBufferArray[1][0] + sBufferArray[1][1] + sBufferArray[1][2] + sBufferArray[1][3] + sBufferArray[1][4] + sBufferArray[1][5] + sBufferArray[1][6] + sBufferArray[1][7] + sBufferArray[1][8] + sBufferArray[1][9] - lY_Min - lY_Max) >> 3;

        return 1;
    }
    return 0;
}

/**
 * @brief  计算 XPT2046 触摸坐标校正系数（注意：只有在LCD和触摸屏间的误差角度非常小时,才能运用下面公式）
 * @param  pDisplayCoordinate ：屏幕人为显示的已知坐标
 * @param  pstrScreenSample ：对已知坐标点触摸时 XPT2046 产生的坐标
 * @param  pCalibrationFactor ：根据人为设定坐标和采样回来的坐标计算出来的屏幕触摸校正系数
 * @retval 计算状态
 *   该返回值为以下值之一：
 *     @arg 1 :计算成功
 *     @arg 0 :计算失败
 */
static uint8_t XPT2046_Calculate_CalibrationFactor(strType_XPT2046_Coordinate *pDisplayCoordinate, strType_XPT2046_Coordinate *pScreenSample, strType_XPT2046_Calibration *pCalibrationFactor) {
    uint8_t ucRet = 1;

    /* K＝ ( X0－X2 )  ( Y1－Y2 )－ ( X1－X2 )  ( Y0－Y2 ) */
    pCalibrationFactor->Divider = ((pScreenSample[0].x - pScreenSample[2].x) * (pScreenSample[1].y - pScreenSample[2].y)) - ((pScreenSample[1].x - pScreenSample[2].x) * (pScreenSample[0].y - pScreenSample[2].y));

    if (pCalibrationFactor->Divider == 0)
        ucRet = 0;

    else {
        /* A＝ (  ( XD0－XD2 )  ( Y1－Y2 )－ ( XD1－XD2 )  ( Y0－Y2 ) )／K	*/
        pCalibrationFactor->An = ((pDisplayCoordinate[0].x - pDisplayCoordinate[2].x) * (pScreenSample[1].y - pScreenSample[2].y)) - ((pDisplayCoordinate[1].x - pDisplayCoordinate[2].x) * (pScreenSample[0].y - pScreenSample[2].y));

        /* B＝ (  ( X0－X2 )  ( XD1－XD2 )－ ( XD0－XD2 )  ( X1－X2 ) )／K	*/
        pCalibrationFactor->Bn = ((pScreenSample[0].x - pScreenSample[2].x) * (pDisplayCoordinate[1].x - pDisplayCoordinate[2].x)) - ((pDisplayCoordinate[0].x - pDisplayCoordinate[2].x) * (pScreenSample[1].x - pScreenSample[2].x));

        /* C＝ ( Y0 ( X2XD1－X1XD2 )+Y1 ( X0XD2－X2XD0 )+Y2 ( X1XD0－X0XD1 ) )／K */
        pCalibrationFactor->Cn = (pScreenSample[2].x * pDisplayCoordinate[1].x - pScreenSample[1].x * pDisplayCoordinate[2].x) * pScreenSample[0].y + (pScreenSample[0].x * pDisplayCoordinate[2].x - pScreenSample[2].x * pDisplayCoordinate[0].x) * pScreenSample[1].y + (pScreenSample[1].x * pDisplayCoordinate[0].x - pScreenSample[0].x * pDisplayCoordinate[1].x) * pScreenSample[2].y;

        /* D＝ (  ( YD0－YD2 )  ( Y1－Y2 )－ ( YD1－YD2 )  ( Y0－Y2 ) )／K	*/
        pCalibrationFactor->Dn = ((pDisplayCoordinate[0].y - pDisplayCoordinate[2].y) * (pScreenSample[1].y - pScreenSample[2].y)) - ((pDisplayCoordinate[1].y - pDisplayCoordinate[2].y) * (pScreenSample[0].y - pScreenSample[2].y));

        /* E＝ (  ( X0－X2 )  ( YD1－YD2 )－ ( YD0－YD2 )  ( X1－X2 ) )／K	*/
        pCalibrationFactor->En = ((pScreenSample[0].x - pScreenSample[2].x) * (pDisplayCoordinate[1].y - pDisplayCoordinate[2].y)) - ((pDisplayCoordinate[0].y - pDisplayCoordinate[2].y) * (pScreenSample[1].x - pScreenSample[2].x));

        /* F＝ ( Y0 ( X2YD1－X1YD2 )+Y1 ( X0YD2－X2YD0 )+Y2 ( X1YD0－X0YD1 ) )／K */
        pCalibrationFactor->Fn = (pScreenSample[2].x * pDisplayCoordinate[1].y - pScreenSample[1].x * pDisplayCoordinate[2].y) * pScreenSample[0].y + (pScreenSample[0].x * pDisplayCoordinate[2].y - pScreenSample[2].x * pDisplayCoordinate[0].y) * pScreenSample[1].y + (pScreenSample[1].x * pDisplayCoordinate[0].y - pScreenSample[0].x * pDisplayCoordinate[1].y) * pScreenSample[2].y;
    }

    return ucRet;
}

/**
 * @brief  在 ILI9341 上显示校正触摸时需要的十字
 * @param  usX ：在特定扫描方向下十字交叉点的X坐标
 * @param  usY ：在特定扫描方向下十字交叉点的Y坐标
 * @retval 无
 */
static void ILI9341_DrawCross(uint16_t usX, uint16_t usY) {
    ILI9341_DrawLine(usX - 10, usY, usX + 10, usY);
    ILI9341_DrawLine(usX, usY - 10, usX, usY + 10);
}

#if 1
/**
 * @brief  XPT2046 触摸屏校准
 * @param	LCD_Mode：指定要校正哪种液晶扫描模式的参数
 * @note  本函数调用后会把液晶模式设置为LCD_Mode
 * @retval 校准结果
 *   该返回值为以下值之一：
 *     @arg 1 :校准成功
 *     @arg 0 :校准失败
 */
uint8_t XPT2046_Touch_Calibrate() {
    uint8_t i;

    uint8_t result = 0;
    // uint16_t usTest_x = 0, usTest_y = 0, usGap_x = 0, usGap_y = 0;

    strType_XPT2046_Coordinate strCrossCoordinate[4], strScreenSample[4];

    strType_XPT2046_Calibration CalibrationFactor;

    LCD_SetColors(BLUE, BLACK);

    /* 设定“十”字交叉点的坐标 */
    strCrossCoordinate[0].x = LCD_X_LENGTH >> 2;
    strCrossCoordinate[0].y = LCD_Y_LENGTH >> 2;

    strCrossCoordinate[1].x = strCrossCoordinate[0].x;
    strCrossCoordinate[1].y = (LCD_Y_LENGTH * 3) >> 2;

    strCrossCoordinate[2].x = (LCD_X_LENGTH * 3) >> 2;
    strCrossCoordinate[2].y = strCrossCoordinate[1].y;

    strCrossCoordinate[3].x = strCrossCoordinate[2].x;
    strCrossCoordinate[3].y = strCrossCoordinate[0].y;

    for (i = 0; i < 4; i++) {
        ILI9341_Clear(0, 0, LCD_X_LENGTH, LCD_Y_LENGTH);

        BSP_DelayUS(300000); // 适当的延时很有必要

        ILI9341_DrawCross(strCrossCoordinate[i].x, strCrossCoordinate[i].y); // 显示校正用的“十”字

        while (!XPT2046_ReadAdc_Smooth_XY(&strScreenSample[i])); // 读取XPT2046数据到变量pCoordinate，当ptr为空时表示没有触点被按下
    }

    XPT2046_Calculate_CalibrationFactor(strCrossCoordinate, strScreenSample, &CalibrationFactor); // 用原始参数计算出 原始参数与坐标的转换系数

    if (CalibrationFactor.Divider == 0) {
        SEGGER_RTT_printf(0, "XPT2046_Touch_Calibrate: Divider is zero\r\n");
    } else {
        /* 校准系数为全局变量 */
        strXPT2046_TouchPara[lcd_scan_mode].dX_X = (CalibrationFactor.An * 1.0) / CalibrationFactor.Divider;
        strXPT2046_TouchPara[lcd_scan_mode].dX_Y = (CalibrationFactor.Bn * 1.0) / CalibrationFactor.Divider;
        strXPT2046_TouchPara[lcd_scan_mode].dX = (CalibrationFactor.Cn * 1.0) / CalibrationFactor.Divider;

        strXPT2046_TouchPara[lcd_scan_mode].dY_X = (CalibrationFactor.Dn * 1.0) / CalibrationFactor.Divider;
        strXPT2046_TouchPara[lcd_scan_mode].dY_Y = (CalibrationFactor.En * 1.0) / CalibrationFactor.Divider;
        strXPT2046_TouchPara[lcd_scan_mode].dY = (CalibrationFactor.Fn * 1.0) / CalibrationFactor.Divider;

        ILI9341_Clear(0, 0, LCD_X_LENGTH, LCD_Y_LENGTH);

        LCD_SetTextColor(GREEN);

        BSP_DelayUS(1000000);
        result = 1;
    }

    return result;
}

#else

void calculate_calibration_params(strType_XPT2046_Coordinate *calib_points_phys,
                                  strType_XPT2046_Coordinate *raw_points, strType_fivePointCalib *calib);

/**
 * @brief  XPT2046 触摸屏校准
 * @param	LCD_Mode：指定要校正哪种液晶扫描模式的参数
 * @note  本函数调用后会把液晶模式设置为LCD_Mode
 * @retval 校准结果
 *   该返回值为以下值之一：
 *     @arg 1 :校准成功
 *     @arg 0 :校准失败
 */

uint8_t XPT2046_Touch_Calibrate() {
    uint8_t i;

    uint8_t result = 0;

    strType_XPT2046_Coordinate strCrossCoordinate[5], strScreenSample[5];

    strType_fivePointCalib CalibrationFactor;

    LCD_SetColors(BLUE, BLACK);

    /* 设定“十”字交叉点的坐标 */
    strCrossCoordinate[0].x = LCD_X_LENGTH >> 2;
    strCrossCoordinate[0].y = LCD_Y_LENGTH >> 2;

    strCrossCoordinate[1].x = strCrossCoordinate[0].x;
    strCrossCoordinate[1].y = (LCD_Y_LENGTH * 3) >> 2;

    strCrossCoordinate[2].x = (LCD_X_LENGTH * 3) >> 2;
    strCrossCoordinate[2].y = strCrossCoordinate[1].y;

    strCrossCoordinate[3].x = strCrossCoordinate[2].x;
    strCrossCoordinate[3].y = strCrossCoordinate[0].y;

    strCrossCoordinate[4].x = LCD_X_LENGTH >> 1;
    strCrossCoordinate[4].y = LCD_Y_LENGTH >> 1;

    for (i = 0; i < 4; i++) {
        ILI9341_Clear(0, 0, LCD_X_LENGTH, LCD_Y_LENGTH);

        BSP_DelayUS(300000); // 适当的延时很有必要

        ILI9341_DrawCross(strCrossCoordinate[i].x, strCrossCoordinate[i].y); // 显示校正用的“十”字

        while (!XPT2046_ReadAdc_Smooth_XY(&strScreenSample[i])); // 读取XPT2046数据到变量pCoordinate，当ptr为空时表示没有触点被按下
    }
    calculate_calibration_params(strCrossCoordinate, strScreenSample, &CalibrationFactor);
}
/**
 * @brief  在 ILI9341 上显示校正触摸时需要的十字
 * @param  calib_points_phys :物理坐标
 * @param  raw_points ：原始坐标
 * @param  calib ：校准因子
 * @retval 无
 */
void calculate_calibration_params(strType_XPT2046_Coordinate *calib_points_phys,
                                  strType_XPT2046_Coordinate *raw_points, strType_fivePointCalib *calib) {
    // 物理点坐标
    int x1 = calib_points_phys[0].x, y1 = calib_points_phys[0].y; // 左上
    int x2 = calib_points_phys[1].x, y2 = calib_points_phys[1].y; // 右上
    int x3 = calib_points_phys[2].x, y3 = calib_points_phys[2].y; // 左下
    int x4 = calib_points_phys[3].x, y4 = calib_points_phys[3].y; // 右下
    int xc = calib_points_phys[4].x, yc = calib_points_phys[4].y; // 中心

    // 原始坐标
    int r1x = raw_points[0].x, r1y = raw_points[0].y;
    int r2x = raw_points[1].x, r2y = raw_points[1].y;
    int r3x = raw_points[2].x, r3y = raw_points[2].y;
    int r4x = raw_points[3].x, r4y = raw_points[3].y;
    int rcx = raw_points[4].x, rcy = raw_points[4].y;

    // 物理间距
    int dx_physical = x2 - x1; // 上边物理宽度
    int dy_physical = y3 - y1; // 左边物理高度

    // 原始间距（分别用上边、下边平均）
    int raw_dx_top = r2x - r1x;
    int raw_dx_bottom = r4x - r3x;
    int raw_dy_left = r3y - r1y;
    int raw_dy_right = r4y - r2y;

    // 计算缩放因子（浮点）
    float kx_top = (float)raw_dx_top / dx_physical;
    float kx_bottom = (float)raw_dx_bottom / dx_physical;
    float ky_left = (float)raw_dy_left / dy_physical;
    float ky_right = (float)raw_dy_right / dy_physical;

    calib->kx = (kx_top + kx_bottom) / 2.0f;
    calib->ky = (ky_left + ky_right) / 2.0f;

    // 基准点（中心）
    calib->xlc = rcx;
    calib->ylc = rcy;
    calib->xc = xc;
    calib->yc = yc;
}

#endif

/**
 * @brief  从FLASH中获取 或 重新校正触摸参数（校正后会写入到SPI FLASH中）
 * @note		若FLASH中从未写入过触摸参数，
 *						会触发校正程序校正LCD_Mode指定模式的触摸参数，此时其它模式写入默认值
 *
 *					若FLASH中已有触摸参数，且不强制重新校正
 *						会直接使用FLASH里的触摸参数值
 *
 *					每次校正时只会更新指定的LCD_Mode模式的触摸参数，其它模式的不变
 * @note  本函数调用后会把液晶模式设置为LCD_Mode
 * @param  forceCal:是否强制重新校正参数，可以为以下值：
 *		@arg 1：强制重新校正
 *		@arg 0：只有当FLASH中不存在触摸参数标志时才重新校正
 * @retval 无
 */
void Calibrate_or_Get_TouchParaWithFlash(uint8_t forceCal) {
    uint32_t para_flag = 0;

    // 读取触摸参数标志
    BSP_FLASH_Read(FLASH_TOUCH_PARA_ADDR + 7 * 4 * lcd_scan_mode, (uint32_t *)&para_flag, 1);

    // 若不存在标志或florceCal=1时，重新校正参数
    if ((para_flag != FLASH_TOUCH_PARA_FLAG_VALUE) || (forceCal == 1)) {
        // 若标志存在，说明原本FLASH内有触摸参数，
        // 先读回所有LCD模式的参数值，以便稍后强制更新时只更新指定LCD模式的参数,其它模式的不变
        if (para_flag == FLASH_TOUCH_PARA_FLAG_VALUE && forceCal == 1) {
            BSP_FLASH_Read(FLASH_TOUCH_PARA_ADDR, (uint32_t *)strXPT2046_TouchPara, 7 * 8);
        }

        // 等待触摸屏校正完毕,更新指定LCD模式的触摸参数值
        while (!XPT2046_Touch_Calibrate());

        // 擦除扇区
        BSP_FLASH_ErasePage(FLASH_TOUCH_PARA_ADDR);
        // 设置触摸参数标志
        para_flag = FLASH_TOUCH_PARA_FLAG_VALUE;
        // 写入触摸参数标志
        strXPT2046_TouchPara[lcd_scan_mode].calibrate_flag = para_flag;
        // 写入最新的触摸参数
        BSP_FLASH_Write(FLASH_TOUCH_PARA_ADDR, (uint32_t *)strXPT2046_TouchPara, 7 * 8);

    } else // 若标志存在且不强制校正，则直接从FLASH中读取
    {
        BSP_FLASH_Read(FLASH_TOUCH_PARA_ADDR, (uint32_t *)strXPT2046_TouchPara, 6 * 8);
#if 0 // 输出调试信息，注意要初始化串口
				{
					
					uint8_t para_flag=0,i;
					float *ulHeadAddres  ;
					
					/* 打印校校准系数 */ 
					XPT2046_INFO ( "从FLASH里读取得的校准系数如下：" );
					
					ulHeadAddres = ( float* ) ( & strXPT2046_TouchPara );

					for ( i = 0; i < 6*8; i ++ )
					{				
						if(i%6==0)
							printf("\r\n");			
									
						printf ( "%12f,", *ulHeadAddres );
						ulHeadAddres++;				
					}
					printf("\r\n");
				}
#endif
    }
}

/**
 * @brief  获取 XPT2046 触摸点（校准后）的坐标
 * @param  pDisplayCoordinate ：该指针存放获取到的触摸点坐标
 * @param  pTouchPara：坐标校准系数
 * @retval 获取情况
 *   该返回值为以下值之一：
 *     @arg 1 :获取成功
 *     @arg 0 :获取失败
 */
uint8_t XPT2046_Get_TouchedPoint(strType_XPT2046_Coordinate *pDisplayCoordinate, strType_XPT2046_TouchPara *pTouchPara) {
    uint8_t ucRet = 1; // 若正常，则返回0

    strType_XPT2046_Coordinate strScreenCoordinate;

    if (XPT2046_ReadAdc_Smooth_XY(&strScreenCoordinate)) {
        pDisplayCoordinate->x = ((pTouchPara[lcd_scan_mode].dX_X * strScreenCoordinate.x) + (pTouchPara[lcd_scan_mode].dX_Y * strScreenCoordinate.y) + pTouchPara[lcd_scan_mode].dX);
        pDisplayCoordinate->y = ((pTouchPara[lcd_scan_mode].dY_X * strScreenCoordinate.x) + (pTouchPara[lcd_scan_mode].dY_Y * strScreenCoordinate.y) + pTouchPara[lcd_scan_mode].dY);

    }

    else
        ucRet = 0; // 如果获取的触点信息有误，则返回0

    return ucRet;
}

/**
 * @brief  触摸屏检测状态机
 * @retval 触摸状态
 *   该返回值为以下值之一：
 *     @arg TOUCH_PRESSED :触摸按下
 *     @arg TOUCH_NOT_PRESSED :无触摸
 */
uint8_t XPT2046_TouchDetect(void) {
    static enumTouchState touch_state = XPT2046_STATE_RELEASE;
    static uint32_t i;
    uint8_t detectResult = TOUCH_NOT_PRESSED;

    switch (touch_state) {
    case XPT2046_STATE_RELEASE:
        if (XPT2046_PENIRQ_Read() == XPT2046_PENIRQ_ActiveLevel) // 第一次出现触摸信号
        {
            touch_state = XPT2046_STATE_WAITING;
            detectResult = TOUCH_NOT_PRESSED;
        } else // 无触摸
        {
            touch_state = XPT2046_STATE_RELEASE;
            detectResult = TOUCH_NOT_PRESSED;
        }
        break;

    case XPT2046_STATE_WAITING:
        if (XPT2046_PENIRQ_Read() == XPT2046_PENIRQ_ActiveLevel) {
            i++;
            // 等待时间大于阈值则认为触摸被按下
            // 消抖时间 = DURIATION_TIME * 本函数被调用的时间间隔
            // 如在定时器中调用，每10ms调用一次，则消抖时间为：DURIATION_TIME*10ms
            if (i > DURIATION_TIME) {
                i = 0;
                touch_state = XPT2046_STATE_PRESSED;
                detectResult = TOUCH_PRESSED;
            } else // 等待时间累加
            {
                touch_state = XPT2046_STATE_WAITING;
                detectResult = TOUCH_NOT_PRESSED;
            }
        } else // 等待时间值未达到阈值就为无效电平，当成抖动处理
        {
            i = 0;
            touch_state = XPT2046_STATE_RELEASE;
            detectResult = TOUCH_NOT_PRESSED;
        }

        break;

    case XPT2046_STATE_PRESSED:
        if (XPT2046_PENIRQ_Read() == XPT2046_PENIRQ_ActiveLevel) // 触摸持续按下
        {
            touch_state = XPT2046_STATE_PRESSED;
            detectResult = TOUCH_PRESSED;
        } else // 触摸释放
        {
            touch_state = XPT2046_STATE_RELEASE;
            detectResult = TOUCH_NOT_PRESSED;
        }
        break;

    default:
        touch_state = XPT2046_STATE_RELEASE;
        detectResult = TOUCH_NOT_PRESSED;
        break;
    }

    return detectResult;
}

/**
 * @brief   检测到触摸中断时调用的处理函数,通过它调用tp_down 和tp_up汇报触摸点
 *	@note 	 本函数需要在while循环里被调用，也可使用定时器定时调用
 *			例如，可以每隔5ms调用一次，消抖阈值宏DURIATION_TIME可设置为2，这样每秒最多可以检测100个点。
 *						可在XPT2046_TouchDown及XPT2046_TouchUp函数中编写自己的触摸应用
 * @param   none
 * @retval  none
 */
void XPT2046_TouchEvenHandler(void) {
    static strType_XPT2046_Coordinate cinfo = {-1, -1, -1, -1};

    if (XPT2046_TouchDetect() == TOUCH_PRESSED) {
        // 获取触摸坐标
        XPT2046_Get_TouchedPoint(&cinfo, strXPT2046_TouchPara);

        // 输出调试信息到串口
        SEGGER_RTT_printf(0, "x=%d,y=%d\n", cinfo.x, cinfo.y);

        // 调用触摸被按下时的处理函数，可在该函数编写自己的触摸按下处理过程
        // XPT2046_TouchDown(&cinfo);

        /*更新触摸信息到pre xy*/
        // cinfo.pre_x = cinfo.x; cinfo.pre_y = cinfo.y;

        SEGGER_RTT_printf(0, "TOUCH DOWN !\n");
    } else {
        // SEGGER_RTT_printf(0, "TOUCH PRESSED!\n");
        // 调用触摸被释放时的处理函数，可在该函数编写自己的触摸释放处理过程
        // XPT2046_TouchUp(&cinfo);

        /*触笔释放，把 xy 重置为负*/
        cinfo.x = -1;
        cinfo.y = -1;
        cinfo.pre_x = -1;
        cinfo.pre_y = -1;
    }
}
