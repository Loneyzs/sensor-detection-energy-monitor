#include "monitor.h"
#include "app_config.h"
#include <string.h>

bool monitor_power_on(const struct monitor *m)
{
	return m->state == MON_RUNNING || m->state == MON_PENDING;
}

void monitor_init(struct monitor *m)
{
	memset(m, 0, sizeof(*m));
	m->params = (struct monitor_params){
		.threshold_mw = MONITOR_THRESHOLD_MW,
		.standby_ms = MONITOR_STANDBY_MS,
	};
	m->state = MON_OFF;
}

bool monitor_enable(struct monitor *m, int64_t now)
{
	if (!m->valid) {
		return false;
	}
	/* 按键恢复供电或重新计时；低功率计时由接通后的新采样启动。 */
	m->state = MON_RUNNING;
	m->previous_ms = now;
	m->have_previous = true;
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
	/* 采样中断时关闭输出，防止用陈旧读数判断连续低功率。 */
	if (m->have_previous && (now < m->previous_ms ||
	    now - m->previous_ms > MONITOR_MAX_SAMPLE_GAP_MS)) {
		m->state = MON_FAULT;
		m->have_previous = false;
		return;
	}
	m->previous_ms = now;
	m->have_previous = true;
	/* 21.6W及以上为正常档；恢复正常功率立即清除本轮待机计时。 */
	if (sample->power_uw >= (int64_t)m->params.threshold_mw * 1000) {
		m->state = MON_RUNNING;
		return;
	}
	if (m->state == MON_RUNNING) {
		m->low_since_ms = now;
		m->state = MON_PENDING;
	}
	/* 10Ω、空载及反向功率均按低功率处理，连续30秒后断电。 */
	if (now - m->low_since_ms >= m->params.standby_ms) {
		monitor_disable(m);
	}
}

const char *monitor_state_name(enum monitor_state state)
{
	static const char *const names[] = {
		"OFF", "RUNNING", "PENDING", "FAULT"
	};
	return names[state];
}
