#ifndef NET_TIME_H
#define NET_TIME_H

#include "stdint.h"
#include "net_wifi.h"

cmd_state_t net_time_mode_set(uint8_t *rx_buf, uint32_t rx_buf_size);
void net_time_init(void);
#endif