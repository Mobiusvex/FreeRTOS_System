#ifndef HWDATAACCESS_H
#define HWDATAACCESS_H
#include "cmsis_os2.h"

enum HW_STATUS {
    HW_STATUS_OK = 0,
    HW_STATUS_ERROR = 1
};

typedef struct {
    uint8_t ConnectionError;
    uint8_t temperature;
    uint8_t humidity;
    uint8_t (*Init)(void);
    uint8_t (*GetHumiTemp)(float *humi, float *temp);
} HW_DHT11_InterfaceTypeDef;

typedef struct {
    HW_DHT11_InterfaceTypeDef DHT11;
} HW_InterfaceTypeDef;

extern HW_InterfaceTypeDef HW_Interface;

#endif
