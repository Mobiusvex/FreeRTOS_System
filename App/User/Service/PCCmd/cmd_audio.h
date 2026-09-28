#ifndef CMD_AUDIO_H
#define CMD_AUDIO_H
#include "stdint.h"
#include "frame.h"

typedef enum {
    AUDIO_SLOT_1 = 1,
    AUDIO_SLOT_2 = 2,
    AUDIO_SLOT_3 = 3,
    AUDIO_SLOT_4 = 4,
    AUDIO_SLOT_5 = 5,
} enum_slot_t;

void AUDIO_HandleStart(const Frame_t *frame); /* 解析总包数，擦Flash */
void AUDIO_HandleData(const Frame_t *frame);  /* 解密，写Flash，回ACK */
void AUDIO_HandleEnd(const Frame_t *frame);   /* 总CRC校验，回ACK */

bool AUDIO_GetInfo(enum_slot_t slot, char *filename, uint32_t *file_size);

bool AUDIO_ReadData(enum_slot_t slot, uint32_t offset, uint8_t *buf, uint32_t len);

#endif