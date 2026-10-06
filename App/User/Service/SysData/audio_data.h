#ifndef AUDIO_DATA_H
#define AUDIO_DATA_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    AUDIO_SLOT_NONE = 0, // 无音频
    AUDIO_SLOT_1 = 1,
    AUDIO_SLOT_2 = 2,
    AUDIO_SLOT_3 = 3,
    AUDIO_SLOT_4 = 4,
    AUDIO_SLOT_5 = 5,
} enum_slot_t;

void AudioNames_LoadAll(void);
void AudioNames_LoadOne(enum_slot_t slot);
bool AudioNames_Get(int slot, char *out, uint8_t out_size);
int AudioNames_Count(void);
bool AUDIO_GetInfo(enum_slot_t slot, char *filename, uint32_t *file_size);
bool AUDIO_ReadData(enum_slot_t slot, uint32_t offset, uint8_t *buf, uint32_t len);
#endif