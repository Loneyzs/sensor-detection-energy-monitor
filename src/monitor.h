#ifndef ENERGY_MONITOR_H
#define ENERGY_MONITOR_H
#include <stdbool.h>
#include <stdint.h>

enum monitor_state { MON_OFF, MON_STARTUP, MON_RUNNING, MON_PENDING,
	MON_WARNING, MON_FAULT, MON_OVERPOWER };
struct measurement { int32_t bus_mv; int32_t current_ua; int64_t power_uw; };
struct monitor_params {
	uint32_t threshold_mw;
	uint32_t standby_ms;
	uint32_t warning_ms;
	uint32_t startup_ms;
	uint32_t max_power_mw;
};
struct monitor {
	struct monitor_params params;
	enum monitor_state state;
	struct measurement reading;
	bool valid;
	bool have_previous;
	int64_t previous_ms;
	int64_t previous_power_uw;
	int64_t startup_until_ms;
	int64_t low_since_ms;
	int64_t warning_since_ms;
	uint64_t energy_uw_ms; /* divide by 3600000 for microWh */
};
void monitor_init(struct monitor *m);
void monitor_sample(struct monitor *m, int64_t now, bool valid,
		    const struct measurement *sample);
bool monitor_enable(struct monitor *m, int64_t now);
void monitor_disable(struct monitor *m);
bool monitor_power_on(const struct monitor *m);
const char *monitor_state_name(enum monitor_state state);
#endif
