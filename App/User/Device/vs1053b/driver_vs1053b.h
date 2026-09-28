#ifndef DRIVER_VS1053B_H
#define DRIVER_VS1053B_H

#include <stdint.h>
#include <stdbool.h>

/* ============ SCI 寄存器地址 ============ */
#define VS1053_SCI_MODE 0x00
#define VS1053_SCI_STATUS 0x01
#define VS1053_SCI_BASS 0x02
#define VS1053_SCI_CLOCKF 0x03
#define VS1053_SCI_DECODE_TIME 0x04
#define VS1053_SCI_AUDATA 0x05
#define VS1053_SCI_WRAM 0x06
#define VS1053_SCI_WRAMADDR 0x07
#define VS1053_SCI_HDAT0 0x08
#define VS1053_SCI_HDAT1 0x09
#define VS1053_SCI_AIADDR 0x0A
#define VS1053_SCI_VOL 0x0B
#define VS1053_SCI_AICTRL0 0x0C
#define VS1053_SCI_AICTRL1 0x0D
#define VS1053_SCI_AICTRL2 0x0E
#define VS1053_SCI_AICTRL3 0x0F

/* ============ SCI_MODE 位定义 ============ */
/* ============ SCI_MODE 位定义（VS1053B 数据手册） ============ */
#define VS1053_SM_DIFF 0x0001
#define VS1053_SM_LAYER12 0x0002
#define VS1053_SM_RESET 0x0004
#define VS1053_SM_CANCEL 0x0008
#define VS1053_SM_EARSPEAKER_LO 0x0010
#define VS1053_SM_TESTS 0x0020
#define VS1053_SM_STREAM 0x0040
#define VS1053_SM_EARSPEAKER_HI 0x0080
#define VS1053_SM_DACT 0x0100
#define VS1053_SM_SDIORD 0x0200
#define VS1053_SM_SDISHARE 0x0400
#define VS1053_SM_SDINEW 0x0800
#define VS1053_SM_ADPCM 0x1000
#define VS1053_SM_ADPCM_HP 0x2000
#define VS1053_SM_LINE1 0x4000
#define VS1053_SM_CLK_RANGE 0x8000

/* ============ 音调控制 ============ */
#define VS1053_BASS_TREBLE_MASK 0xFFFF

/* ============ 接口函数 ============ */
bool VS1053_Init(void);
void VS1053_Reset(void);
void VS1053_SoftReset(void);
void VS1053_SetVolume(uint8_t left, uint8_t right);
void VS1053_SetBassTreble(uint8_t bass, uint8_t treble);

/* SCI 寄存器读写（命令接口） */
void VS1053_WriteSci(uint8_t addr, uint16_t data);
uint16_t VS1053_ReadSci(uint8_t addr);

/* SDI 数据写入（音频数据接口） */
bool VS1053_WriteSdi(const uint8_t *data, uint16_t len);
bool VS1053_WriteSdiBlocking(const uint8_t *data, uint16_t len);

/* 状态查询 */
bool VS1053_IsReady(void);
uint16_t VS1053_GetDecodeTime(void);
uint16_t VS1053_GetBitRate(void);
uint16_t VS1053_GetEndFillByte(void);

/* 结束播放 */
void VS1053_StopPlay(void);

#endif /* DRIVER_VS1053B_H */