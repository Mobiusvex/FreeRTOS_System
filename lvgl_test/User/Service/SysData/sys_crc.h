#ifndef __SYS_CRC_H__
#define __SYS_CRC_H__

#include "stdint.h"

uint16_t CRC16_WithUniqueID_Software(const uint8_t *pData, uint32_t dataLen);
#endif