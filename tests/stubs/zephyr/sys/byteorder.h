#ifndef TEST_BYTEORDER_H
#define TEST_BYTEORDER_H
#include <stdint.h>
static inline uint16_t sys_get_be16(const uint8_t *p) { return ((uint16_t)p[0] << 8) | p[1]; }
#endif
