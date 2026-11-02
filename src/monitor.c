#include "monitor.h"
#include "app_config.h"
#include <string.h>

bool monitor_power_on(const struct monitor *m)
{
	return m->state >= MON_STARTUP && m->state <= MON_WARNING;
}

void monitor_init(struct monitor *m)
{
	memset(m, 0, sizeof(*m));
	m->params = (struct monitor_params){
		.threshold_mw = MONITOR_THRESHOLD_MW,
		.standby_ms = MONITOR_STANDBY_MS,
		.warning_ms = MONITOR_WARNING_MS,
		.startup_ms = MONITOR_STARTUP_MS,
		.max_power_mw = MONITOR_MAX_POWER_MW,
	};
	m->state = MON_OFF;
}

bool monitor_enable(struct monitor *m, int64_t now)
{
	if (!m->valid) {
		return false;
	}
	bool was_on = monitor_power_on(m);
	m->state = MON_STARTUP;
	m->startup_until_ms = now + m->params.startup_ms;
	if (!was_on) {
		m->have_previous = false;
	}
	return true;
}

void monitor_disable(struct monitor *m)
{
	m->state = MON_OFF;
	m->have_previous = false;
}

void monitor_sample(struct monitor *m, int64_t now, bool valid,
		    const struct measurement *sample)
{
	m->valid = valid;
	if (!valid) {
		m->state = MON_FAULT;
		m->have_previous = false;
		return;
	}
	m->reading = *sample;
	if (!monitor_power_on(m)) {
		m->have_previous = false;
		return;
	}
	/* A missing acquisition interval breaks timing and integration continuity. */
	if (m->have_previous && (now <= m->previous_ms || now - m->previous_ms > 500)) {
		m->state = MON_FAULT;
		m->have_previous = false;
		return;
	}
	int64_t power = sample->power_uw > 0 ? sample->power_uw : 0;
	if (m->have_previous) {
		m->energy_uw_ms += (uint64_t)((power + m->previous_power_uw) *
					    (now - m->previous_ms) / 2);
	}
	m->previous_ms = now;
	m->previous_power_uw = power;
	m->have_previous = true;
	/* Apply the power limit during startup and countdown as well as normal run. */
	if (power >= (int64_t)m->params.max_power_mw * 1000) {
		m->state = MON_OVERPOWER;
		m->have_previous = false;
		return;
	}
	if (m->state == MON_STARTUP) {
		if (now < m->startup_until_ms) {
			return;
		}
		m->state = MON_RUNNING;
	}
	if (power >= (int64_t)m->params.threshold_mw * 1000) {
		m->state = MON_RUNNING;
		return;
	}
	if (m->state == MON_RUNNING) {
		m->low_since_ms = now;
		m->state = MON_PENDING;
	}
	if (m->state == MON_PENDING && now - m->low_since_ms >= m->params.standby_ms) {
		m->state = MON_WARNING;
		m->warning_since_ms = now;
	}
	if (m->state == MON_WARNING && now - m->warning_since_ms >= m->params.warning_ms) {
		monitor_disable(m);
	}
}

const char *monitor_state_name(enum monitor_state state)
{
	static const char *const names[] = {
		"OFF", "STARTUP", "RUNNING", "PENDING", "WARNING", "FAULT", "OVERPOWER"
	};
	return names[state];
}
