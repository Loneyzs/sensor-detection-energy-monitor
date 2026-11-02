#include "ui_model.h"
#include "app_config.h"
#include <stdio.h>
#include <string.h>

bool monitor_resistance_mohm(const struct monitor *m, uint64_t *value)
{
	if (!m->valid || !monitor_power_on(m) || m->reading.bus_mv <= 0 ||
	    m->reading.current_ua < MONITOR_RESISTANCE_MIN_UA) {
		return false;
	}
	*value = (uint64_t)m->reading.bus_mv * 1000000 / m->reading.current_ua;
	return true;
}

static uint32_t seconds_left(int64_t end, int64_t now)
{
	return end > now ? (uint32_t)((end - now + 999) / 1000) : 0;
}

static void fixed(char *out, size_t size, int64_t value, unsigned int decimals,
		 unsigned int divisor)
{
	/* All inputs are bounded voltage/current/power or positive accumulated energy. */
	uint64_t magnitude = value < 0 ? (uint64_t)(-value) : (uint64_t)value;
	snprintf(out, size, "%s%llu.%0*llu", value < 0 ? "-" : "",
		 (unsigned long long)(magnitude / divisor), (int)decimals,
		 (unsigned long long)(magnitude % divisor));
}

void ui_build(const struct monitor *m, int64_t now, struct ui_view *view)
{
	memset(view, 0, sizeof(*view));
	view->warning = m->state == MON_WARNING;
	if (view->warning) {
		view->remaining_s = seconds_left(m->warning_since_ms + m->params.warning_ms, now);
		return;
	}
	char u[24], i[24], p[24], e[24];
	if (m->valid) {
		fixed(u, sizeof(u), (m->reading.bus_mv + 5) / 10, 2, 100);
		int64_t ma = m->reading.current_ua;
		ma = ma < 0 ? -((-ma + 500) / 1000) : (ma + 500) / 1000;
		fixed(i, sizeof(i), ma, 3, 1000);
		int64_t mw = m->reading.power_uw;
		mw = mw < 0 ? -((-mw + 500) / 1000) : (mw + 500) / 1000;
		fixed(p, sizeof(p), mw, 3, 1000);
	} else {
		strcpy(u, "--"); strcpy(i, "--"); strcpy(p, "--");
	}
	snprintf(view->lines[0], UI_LINE_BYTES, "U:%sV I:%sA", u, i);
	snprintf(view->lines[1], UI_LINE_BYTES, "P: %s W", p);
	/* 0.0001 Wh increments, computed from the full-resolution accumulator. */
	fixed(e, sizeof(e), m->energy_uw_ms / 360000000, 4, 10000);
	snprintf(view->lines[2], UI_LINE_BYTES, "E: %s Wh", e);
	uint64_t resistance;
	if (monitor_resistance_mohm(m, &resistance)) {
		char r[24];
		fixed(r, sizeof(r), (resistance + 50) / 100, 1, 10);
		snprintf(view->lines[3], UI_LINE_BYTES, "R: %s Ohm", r);
	} else {
		strcpy(view->lines[3], "R: -- Ohm");
	}
	switch (m->state) {
	case MON_OFF:
		strcpy(view->lines[4], "已断电");
		strcpy(view->lines[5], "按键恢复供电");
		break;
	case MON_STARTUP:
		snprintf(view->lines[4], UI_LINE_BYTES, "启动宽限 %us",
			 seconds_left(m->startup_until_ms, now));
		break;
	case MON_RUNNING:
		strcpy(view->lines[4], "状态 供电中");
		break;
	case MON_PENDING:
		snprintf(view->lines[4], UI_LINE_BYTES, "待机计时 %u/%us",
			 (unsigned int)((now - m->low_since_ms) / 1000), m->params.standby_ms / 1000);
		break;
	case MON_FAULT:
		strcpy(view->lines[4], m->valid ? "异常已解除" : "传感器异常");
		strcpy(view->lines[5], m->valid ? "按键恢复供电" : "输出已关闭");
		break;
	case MON_OVERPOWER:
		strcpy(view->lines[4], "功率超限 已断电");
		strcpy(view->lines[5], "按键恢复供电");
		break;
	case MON_WARNING:
		break;
	}
	if (view->lines[5][0] == '\0') {
		snprintf(view->lines[5], UI_LINE_BYTES, "阈值%u.%02uW 等待%us",
			 m->params.threshold_mw / 1000, (m->params.threshold_mw % 1000) / 10,
			 m->params.standby_ms / 1000);
	}
}
