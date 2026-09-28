#include "tools_func.h"

/**
 * @brief 小端序u8数组转换为u32
 * @param p 小端序u8数组
 * @retval uint32_t 小端序u8数组转换为u32
 */
uint32_t get_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
           | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/**
 * @brief u32转换为小端序u8数组
 * @param p 小端序u8数组
 * @param v u32
 * @retval void 无返回值
 */
void put_u32_le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/**
 * @brief 小端序u8数组转换为u16
 * @param p 小端序u8数组
 * @retval uint16_t 小端序u8数组转换为u16
 */
uint16_t get_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}