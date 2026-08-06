#ifndef DHT11_DRIVER_H
#define DHT11_DRIVER_H

enum DHT11_STATUS {
    DHT11_OK = 0,
    DHT11_ERROR,
    DHT11_TIMEOUT,
    DHT11_INVALID_DATA,
};
void DHT11_Init(void);
int DHT11_Read(float *hum, float *temp);

#endif // DHT11_DRIVER_H