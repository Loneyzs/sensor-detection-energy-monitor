#ifndef TEST_KERNEL_H
#define TEST_KERNEL_H
#include <stdint.h>
#define BUILD_ASSERT(cond, msg) _Static_assert(cond, msg)
int64_t k_uptime_get(void);
void k_msleep(int ms);
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
static inline void k_yield(void) {}
#endif
