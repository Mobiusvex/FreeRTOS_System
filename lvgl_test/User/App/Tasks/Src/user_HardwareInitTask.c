#include "tim.h"

#include "user_HardwareInitTask.h"
#include "HWDataAccess.h"
#include "sys_data.h"
#include "debug_func.h"
#include "driver_lcd.h"
#include "driver_xpt2046.h"
#include "lvgl.h"
#include "hal/lv_hal_tick.h"
#include "lv_port_disp_template.h"
#include "lv_port_indev_template.h"
#include "tim.h"
#include "ui.h"
/**
 * @brief Hardware initialization task
 * @param pvParameters Task parameters
 * @retval None
 */
void hardwareInitTask(void *pvParameters) {
    uint32_t lock_state = osKernelLock();
    uint8_t count = 3;
    SYS_DATA_Init();
    HAL_TIM_Base_Start(&htim7);

    SEGGER_RTT_Init();
    RTT_PRINTF("RTT Init OK\n");

    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();

    count = 3;
    while (count && HW_Interface.DHT11.ConnectionError) {
        count--;
        HW_Interface.DHT11.ConnectionError = HW_Interface.DHT11.Init();
    }
    count = 3;
    while (count && HW_Interface.MPU6050.ConnectionError) {
        count--;
        HW_Interface.MPU6050.ConnectionError = HW_Interface.MPU6050.Init();
    }
    count = 3;
    while (count && HW_Interface.ESP8266.ConnectionError) {
        count--;
        HW_Interface.ESP8266.ConnectionError = HW_Interface.ESP8266.Init();
    }
    HAL_TIM_Base_Start_IT(&htim2);
    // lv_obj_t *switch_obj = lv_switch_create(lv_scr_act());

    // lv_obj_set_size(switch_obj, 50, 20);
    // lv_obj_align(switch_obj, LV_ALIGN_TOP_LEFT, 10, 10);

    ui_init();
    osKernelRestoreLock(lock_state);
    osThreadExit();
}