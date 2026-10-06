#ifndef CMD_AUDIO_H
#define CMD_AUDIO_H
#include "stdint.h"
#include "frame.h"
#include "sys_data.h"

void AUDIO_HandleStart(const Frame_t *frame); /* 解析总包数，擦Flash */
void AUDIO_HandleData(const Frame_t *frame);  /* 解密，写Flash，回ACK */
void AUDIO_HandleEnd(const Frame_t *frame);   /* 总CRC校验，回ACK */
void AUDIO_HandlePlay(const Frame_t *frame);  /* 播放音频 */
void audio_sys_data_update(void);
#endif