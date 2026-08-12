#ifndef DHT11_DRIVER_H
#define DHT11_DRIVER_H

#include "sys_defs.h"

void DHT11_Init(void);
SYS_StatusTypeDef DHT11_Read(float *hum, float *temp);

#endif // DHT11_DRIVER_H