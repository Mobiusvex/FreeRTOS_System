#ifndef NET_WEATHER_H
#define NET_WEATHER_H

#include "stdint.h"
#include "net_wifi.h"

cmd_state_t net_weather_mode_set(uint8_t *rx_buf, uint32_t rx_buf_size);
void net_weather_init(void);
void set_weather_city(const char *city);
#endif