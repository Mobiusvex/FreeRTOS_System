#include "bsp_init.h"
#include "bsp_spi.h"
#include "bsp_iic.h"
#include "bsp_gpio.h"
#include "bsp_uart.h"
void bsp_init(void) {
    BSP_GPIO_Init();
    BSP_SPI_Init(BSP_SPI_BUS_XPT2046, NULL);
    BSP_SPI_Init(BSP_SPI_BUS_1, NULL);
    BSP_SPI_Init(BSP_SPI_BUS_2, NULL);
    BSP_UART_Init(BSP_UART_PC, NULL);
    BSP_UART_Init(BSP_UART_ESP8266, NULL);
}