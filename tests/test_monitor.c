#include "monitor.h"
#include <assert.h>
#include <stdio.h>

static void feed(struct monitor *m, int64_t now, int64_t power)
{
	struct measurement sample = {.bus_mv = 5000, .current_ua = power / 5,
		.power_uw = power};
	monitor_sample(m, now, true, &sample);
}
static void start(struct monitor *m)
{
	monitor_init(m);
	assert(!monitor_enable(m, 0));
	feed(m, 0, 0);
	assert(monitor_enable(m, 0));
}
int main(void)
{
	struct monitor m;
	start(&m);
	/* Constant 1W for exactly one hour, real elapsed time and 100ms samples. */
	for (int64_t t = 0; t <= 3600000; t += 100) { feed(&m, t, 1000000); }
	assert(m.energy_uw_ms / 3600000 == 1000000); /* 1Wh */
	assert(m.state == MON_RUNNING);
	/* Cancelling a countdown while still powered must preserve energy continuity. */
	assert(monitor_enable(&m, 3600000));
	feed(&m, 3600100, 1000000);
	assert(m.energy_uw_ms == (uint64_t)1000000 * 3600100);
	m.state = MON_RUNNING; /* isolate the threshold boundary from startup grace */
	/* Threshold equality breaks the consecutive low-power window. */
	feed(&m, 3600200, 0);
	assert(m.state == MON_PENDING);
	feed(&m, 3600300, 300000);
	assert(m.state == MON_RUNNING);
	/* Startup grace, standby duration, warning duration and final cutoff. */
	start(&m);
	for (int64_t t = 0; t < 33000; t += 100) { feed(&m, t, 0); }
	assert(m.state == MON_PENDING);
	feed(&m, 33000, 0);
	assert(m.state == MON_WARNING);
	feed(&m, 33100, 1000000);
	assert(m.state == MON_RUNNING); /* Load resumed during warning. */
	for (int64_t t = 33200; t <= 68200; t += 100) { feed(&m, t, 0); }
	assert(m.state == MON_OFF);
	assert(!monitor_power_on(&m));
	assert(monitor_enable(&m, 68300));
	assert(m.state == MON_STARTUP);
	/* Any failed acquisition cuts power, invalidates sample, and latches fault. */
	monitor_sample(&m, 68400, false, NULL);
	assert(m.state == MON_FAULT && !m.valid && !monitor_power_on(&m));
	assert(!monitor_enable(&m, 68401));
	uint64_t energy = m.energy_uw_ms;
	feed(&m, 68500, 1000000);
	assert(m.state == MON_FAULT && m.energy_uw_ms == energy);
	assert(monitor_enable(&m, 68501));
	feed(&m, 68600, -1000000);
	feed(&m, 68700, -1000000);
	assert(m.energy_uw_ms == energy); /* reverse power does not count as consumption */
	feed(&m, 70000, 1000000);
	assert(m.state == MON_FAULT); /* Missing interval is never integrated. */
	/* PA30 cancellation restarts grace/countdown. */
	start(&m);
	m.params.startup_ms = 0;
	assert(monitor_enable(&m, 0));
	feed(&m, 0, 0);
	assert(m.state == MON_PENDING);
	assert(monitor_enable(&m, 100));
	assert(m.state == MON_STARTUP);
	monitor_disable(&m);
	feed(&m, 200, 1000000);
	assert(m.state == MON_OFF);
	/* The 2W upper boundary trips even within startup grace, and latches. */
	start(&m);
	assert(m.params.max_power_mw == 2000);
	feed(&m, 0, 1999999);
	assert(monitor_power_on(&m));
	feed(&m, 100, 2000000);
	assert(m.state == MON_OVERPOWER && !monitor_power_on(&m));
	energy = m.energy_uw_ms;
	feed(&m, 200, 0);
	assert(m.state == MON_OVERPOWER && m.energy_uw_ms == energy);
	assert(monitor_enable(&m, 201));
	feed(&m, 300, 2100000);
	assert(m.state == MON_OVERPOWER); /* A still-excessive load trips again. */
	/* A high-power sample also overrides a pending low-power warning. */
	start(&m);
	m.state = MON_WARNING;
	m.warning_since_ms = 0;
	feed(&m, 100, 2000001);
	assert(m.state == MON_OVERPOWER && !monitor_power_on(&m));
	monitor_sample(&m, 200, false, NULL);
	assert(m.state == MON_FAULT); /* Acquisition errors remain independently active. */
	puts("monitor: all tests passed");
}
