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
		/* 电压保留0.01V，电流保留0.1mA，功率保留0.001W。 */
		fixed(v, sizeof(v), (m->reading.bus_mv + 5) / 10, 2, 100);
		int64_t current = m->reading.current_ua;
		current = current < 0 ? -((-current + 50) / 100) : (current + 50) / 100;
		fixed(i, sizeof(i), current, 1, 10);
		int64_t power = m->reading.power_uw;
		power = power < 0 ? -((-power + 500) / 1000) : (power + 500) / 1000;
		fixed(p, sizeof(p), power, 3, 1000);
	} else {
		strcpy(v, "--"); strcpy(i, "--"); strcpy(p, "--");
	}
	snprintf(view->lines[0], UI_LINE_BYTES, "V: %s V", v);
	snprintf(view->lines[1], UI_LINE_BYTES, "I: %s mA", i);
	snprintf(view->lines[2], UI_LINE_BYTES, "P: %s W", p);

	switch (m->state) {
	case MON_OFF:
		strcpy(view->lines[3], "已断电");
		strcpy(view->lines[4], "按键恢复供电");
		break;
	case MON_RUNNING:
		strcpy(view->lines[3], "正常供电");
		break;
	case MON_PENDING:
		snprintf(view->lines[3], UI_LINE_BYTES, "待机计时 %u/%u秒",
			 (unsigned int)((now - m->low_since_ms) / 1000), m->params.standby_ms / 1000);
		strcpy(view->lines[4], "按键重新计时");
		break;
	case MON_FAULT:
		strcpy(view->lines[3], m->valid ? "异常已解除" : "传感器异常");
		strcpy(view->lines[4], m->valid ? "按键恢复供电" : "输出已关闭");
		break;
	}
}
