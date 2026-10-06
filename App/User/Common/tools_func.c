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

/* ============================================================
 *  CRC32（标准反射算法，与 Python binascii.crc32 结果一致）
 *  多项式: 0xEDB88320，初值 0xFFFFFFFF，输出异或 0xFFFFFFFF
 * ============================================================ */
uint32_t Soft_CRC32(const uint8_t *data, uint32_t len) {
    uint32_t crc = 0xFFFFFFFFU;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int k = 0; k < 8; k++) {
            crc = (crc & 1) ? (0xEDB88320U ^ (crc >> 1)) : (crc >> 1);
        }
    }
    return crc ^ 0xFFFFFFFFU;
}