#ifndef TOOLS_FUNC_H
#define TOOLS_FUNC_H
#include <stdint.h>
uint32_t get_u32_le(const uint8_t *p);
void put_u32_le(uint8_t *p, uint32_t v);
uint16_t get_u16_le(const uint8_t *p);
/* 直接对一段 buffer 计算协议规定的 CRC32（取低16位） */
uint32_t Soft_CRC32(const uint8_t *data, uint32_t len);

#endif // TOOLS_FUNC_H