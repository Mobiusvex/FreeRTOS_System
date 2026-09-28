#ifndef TOOLS_FUNC_H
#define TOOLS_FUNC_H
#include <stdint.h>
uint32_t get_u32_le(const uint8_t *p);
void put_u32_le(uint8_t *p, uint32_t v);
uint16_t get_u16_le(const uint8_t *p);

#endif // TOOLS_FUNC_H