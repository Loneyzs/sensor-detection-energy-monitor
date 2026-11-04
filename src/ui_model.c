#include "ui_model.h"
#include <stdio.h>
#include <string.h>

static void fixed(char *out, size_t size, int64_t value, unsigned int decimals,
                 unsigned int divisor)
{
	/* 输入已经按显示精度缩放；保留负号，便于调试时发现电流方向接反。 */
	uint64_t magnitude = value < 0 ? (uint64_t)(-value) : (uint64_t)value;
	snprintf(out, size, "%s%llu.%0*llu", value < 0 ? "-" : "",
		 (unsigned long long)(magnitude / divisor), (int)decimals,
		 (unsigned long long)(magnitude % divisor));
}

void ui_build(const struct monitor *m, int64_t now, struct ui_view *view)
{
	memset(view, 0, sizeof(*view));
	char v[24], i[24], p[24];
	if (m->valid) {
		/* 电压保留0.01V、电流保留0.01A、功率保留0.01W，适配ACS712的安培级负载。 */
		fixed(v, sizeof(v), (m->reading.bus_mv + 5) / 10, 2, 100);
		int64_t current = m->reading.current_ua;
		current = current < 0 ? -((-current + 5000) / 10000) : (current + 5000) / 10000;
		fixed(i, sizeof(i), current, 2, 100);
		int64_t power = m->reading.power_uw;
		power = power < 0 ? -((-power + 5000) / 10000) : (power + 5000) / 10000;
		fixed(p, sizeof(p), power, 2, 100);
	} else {
		strcpy(v, "--"); strcpy(i, "--"); strcpy(p, "--");
	}
	snprintf(view->lines[0], UI_LINE_BYTES, "V: %s V", v);
	snprintf(view->lines[1], UI_LINE_BYTES, "I: %s A", i);
	snprintf(view->lines[2], UI_LINE_BYTES, "P: %s W", p);
	char r[24];
	if (m->valid && monitor_power_on(m) && m->reading.bus_mv > 0 &&
	    m->reading.current_ua >= 1000) {
		/* R=U/I：mV÷µA乘1000得到Ω，再乘10保留0.1Ω。
		 * 使用未按显示精度舍入的读数，并在整数除法前加半个除数实现四舍五入。
		 * 断电、反向或低于1mA时显示--，避免零点附近相除造成阻值跳变。
		 */
		int64_t resistance = ((int64_t)m->reading.bus_mv * 10000 +
				      m->reading.current_ua / 2) / m->reading.current_ua;
		fixed(r, sizeof(r), resistance, 1, 10);
	} else {
		strcpy(r, "--");
	}
	snprintf(view->lines[3], UI_LINE_BYTES, "R: %s Ohm", r);

	switch (m->state) {
	case MON_OFF:
		strcpy(view->lines[4], "已断电");
		strcpy(view->lines[5], "按键恢复供电");
		break;
	case MON_RUNNING:
		strcpy(view->lines[4], "正常供电");
		break;
	case MON_PENDING:
		snprintf(view->lines[4], UI_LINE_BYTES, "待机计时 %u/%u秒",
			 (unsigned int)((now - m->low_since_ms) / 1000), m->params.standby_ms / 1000);
		strcpy(view->lines[5], "按键重新计时");
		break;
	case MON_FAULT:
		strcpy(view->lines[4], m->valid ? "异常已解除" : "传感器异常");
		strcpy(view->lines[5], m->valid ? "按键恢复供电" : "输出已关闭");
		break;
	}
}
