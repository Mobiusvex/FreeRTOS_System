#ifndef __DRIVER_LED_H__
#define __DRIVER_LED_H__
#include "stdint.h"

typedef enum {
    LED_BLUE = 0,
    LED_RED,
    LED_GREEN,
    LED_NUM
} LED_NAME_T;

typedef enum {
    LED_OFF,
    LED_ON,
    LED_TOGGLE
} LED_STATE_T;

void led_set_state(LED_NAME_T led_name, LED_STATE_T led_state);
void led_get_state(LED_NAME_T led_name, LED_STATE_T *led_state);
#endif
