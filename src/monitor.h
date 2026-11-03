#ifndef ENERGY_MONITOR_H
#define ENERGY_MONITOR_H
#include <stdbool.h>
#include <stdint.h>

/* 两档负载对应正常供电和待机计时；另外保留断电、采集故障状态。 */
enum monitor_state { MON_OFF, MON_RUNNING, MON_PENDING, MON_FAULT };
/* 统一采用整数物理量：电压mV、电流µA、功率µW，避免浮点换算。 */
struct measurement { int32_t bus_mv; int32_t current_ua; int64_t power_uw; };
struct monitor_params {
	uint32_t threshold_mw;
	uint32_t standby_ms;
};
struct monitor {
	struct monitor_params params;
	enum monitor_state state;
	struct measurement reading;
	bool valid;
	bool have_previous;
	int64_t previous_ms;
	int64_t low_since_ms;
};
void monitor_init(struct monitor *m);
void monitor_sample(struct monitor *m, int64_t now, bool valid,
		    const struct measurement *sample);
bool monitor_enable(struct monitor *m, int64_t now);
void monitor_disable(struct monitor *m);
bool monitor_power_on(const struct monitor *m);
const char *monitor_state_name(enum monitor_state state);
#endif
