#include "driver_led.h"
#include "bsp_gpio.h"
#include "FreeRTOS.h"
#include "task.h"

static const BSP_GPIO_Name_t s_led_map[LED_NUM] = {
    [LED_BLUE] = {BSP_GPIO_LED_BLUE},
    [LED_RED] = {BSP_GPIO_LED_RED},
    [LED_GREEN] = {BSP_GPIO_LED_GREEN},
};

/**
 * @brief 设置LED状态
 * @param led_name LED名称
 * @param led_state LED状态
 * @return void
 * @note 不能多个任务设置LED_TOGGLE
 */
void led_set_state(LED_NAME_T led_name, LED_STATE_T led_state) {
    if (led_name >= LED_NUM) return;
    if (led_state == LED_TOGGLE) {
        BSP_GPIO_Toggle(s_led_map[led_name]);
    } else {
        BSP_GPIO_Write(s_led_map[led_name], (led_state == LED_ON) ? BSP_GPIO_LOW : BSP_GPIO_HIGH);
    }
}

void led_get_state(LED_NAME_T led_name, LED_STATE_T *led_state) {
    if (led_name >= LED_NUM) return;
    *led_state = (BSP_GPIO_Read(s_led_map[led_name]) == BSP_GPIO_HIGH) ? LED_OFF : LED_ON;
}
