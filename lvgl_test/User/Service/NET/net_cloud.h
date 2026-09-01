#ifndef NET_CLOUD_H
#define NET_CLOUD_H
#include "stdint.h"
#include "stdbool.h"
#include "net_wifi.h"
#include "driver_led.h"

typedef struct {
    char temp;
    char humi;
    int16_t yaw;
    int16_t pitch;
    int16_t roll;
    LED_STATE_T led;
} cloud_updata_t;

typedef struct {
    LED_STATE_T led;
} cloud_setdata_t;

void net_cloud_init(void);
cmd_state_t net_cloud_mode_set(uint8_t *rx_buf, uint32_t rx_buf_size);
void net_cloud_data_update(const cloud_updata_t *p_data);
cmd_state_t cloud_setdata_analysis(uint8_t *buffer, uint32_t rx_len);
cloud_setdata_t net_cloud_get_setdata(void);
#endif // NET_CLOUD_H