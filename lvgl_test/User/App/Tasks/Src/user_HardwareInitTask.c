#include "tim.h"

#include "user_HardwareInitTask.h"

#include "HWDataAccess.h"

#include "SEGGER_RTT.h"
#include "lcd_driver.h"
#include "xpt2046_driver.h"
#include "lvgl.h"
#include "hal/lv_hal_tick.h"
#include "lv_port_disp_template.h"
#include "lv_port_indev_template.h"
#include "tim.h"

/**
 * @brief Hardware initialization task
 * @param pvParameters Task parameters
 * @retval None
 */
void hardwareInitTask(void *pvParameters) {
    uint32_t lock_state = osKernelLock();
    uint8_t count = 3;
    HAL_TIM_Base_Start(&htim7);

    SEGGER_RTT_Init();
    XPT2046_CS_DISABLE();
    SEGGER_RTT_printf(0, "RTT Init OK\n");

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

    HAL_TIM_Base_Start_IT(&htim2);
    lv_obj_t *switch_obj = lv_switch_create(lv_scr_act());

    lv_obj_set_size(switch_obj, 50, 20);
    lv_obj_align(switch_obj, LV_ALIGN_TOP_LEFT, 10, 10);
    osKernelRestoreLock(lock_state);
    osThreadExit();
}