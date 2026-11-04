#ifndef TEST_KERNEL_H
#define TEST_KERNEL_H
#include <stdint.h>
#define BUILD_ASSERT(cond, msg) _Static_assert(cond, msg)
int64_t k_uptime_get(void);
void k_msleep(int ms);
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
static inline void k_yield(void) {}
#define K_MSEC(ms) (ms)
#define K_POLL_TYPE_SIGNAL 1
#define K_POLL_MODE_NOTIFY_ONLY 0
#define K_POLL_STATE_NOT_READY 0
struct k_poll_signal { unsigned int signaled; int result; };
struct k_poll_event { int state; struct k_poll_signal *signal; };
static inline void k_poll_signal_init(struct k_poll_signal *s) { s->signaled = 0; s->result = 0; }
static inline void k_poll_signal_reset(struct k_poll_signal *s) { s->signaled = 0; }
static inline void k_poll_signal_check(struct k_poll_signal *s, unsigned int *signaled, int *result)
{ *signaled = s->signaled; *result = s->result; }
static inline void k_poll_event_init(struct k_poll_event *e, int type, int mode, struct k_poll_signal *s)
{ (void)type; (void)mode; e->state = 0; e->signal = s; }
int k_poll(struct k_poll_event *events, int count, int timeout_ms);
#endif
