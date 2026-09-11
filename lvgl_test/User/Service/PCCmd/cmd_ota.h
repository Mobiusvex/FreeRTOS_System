#ifndef CMD_OTA_H
#define CMD_OTA_H
#include "stdint.h"
#include "frame.h"

void OTA_HandleStart(const Frame_t *frame); /* 解析总包数，擦Flash */
void OTA_HandleData(const Frame_t *frame);  /* 解密，写Flash，回ACK */
void OTA_HandleEnd(const Frame_t *frame);   /* 总CRC校验，回ACK */
#endif