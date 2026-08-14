#ifndef driver_dht11_H
#define driver_dht11_H

#include "sys_defs.h"

void DHT11_Init(void);
SYS_StatusTypeDef DHT11_Read(float *hum, float *temp);

#endif // driver_dht11_H