#include "monitor.h"
#include <assert.h>
#include <stdio.h>

#define RUN_UW 211200  /* 12V、680Ω：0.1mA分辨率下约17.6mA。 */
#define LOW_UW 72000   /* 12V、2kΩ：6.0mA。 */

static void feed(struct monitor *m, int64_t now, int64_t power)
{
	struct measurement sample = {.bus_mv = 12000, .current_ua = power / 12,
		.power_uw = power};
	monitor_sample(m, now, true, &sample);
}

static void start(struct monitor *m)
{
	monitor_init(m);
	assert(m->state == MON_OFF && !monitor_power_on(m));
	assert(!monitor_enable(m, 0));
	feed(m, 0, 0);
	assert(monitor_enable(m, 0));
}

int main(void)
{
	struct monitor m;
	start(&m);
	assert(m.params.threshold_mw == 140 && m.params.standby_ms == 30000);
	/* 680Ω持续运行一分钟，跨过待机时长后仍保持供电。 */
	for (int64_t t = 0; t <= 60000; t += 100) { feed(&m, t, RUN_UW); }
	assert(m.state == MON_RUNNING && monitor_power_on(&m));

	/* 从680Ω切至2kΩ，满30秒才断电；后续读数恢复也保持断电。 */
	for (int64_t t = 60100; t < 90100; t += 100) { feed(&m, t, LOW_UW); }
	assert(m.state == MON_PENDING && monitor_power_on(&m));
	feed(&m, 90100, LOW_UW);
	assert(m.state == MON_OFF && !monitor_power_on(&m));
	feed(&m, 90200, RUN_UW);
	assert(m.state == MON_OFF);
	assert(monitor_enable(&m, 90200));
	feed(&m, 90300, RUN_UW);
	assert(m.state == MON_RUNNING);

	/* 直接接2kΩ也从首次采样计30秒，没有启动宽限和额外提醒延时。 */
	start(&m);
	for (int64_t t = 0; t < 30000; t += 100) { feed(&m, t, LOW_UW); }
	assert(m.state == MON_PENDING);
	feed(&m, 30000, LOW_UW);
	assert(m.state == MON_OFF);

	/* 阈值相等归正常档，低于阈值才计时；正常采样会清除旧计时。 */
	start(&m);
	feed(&m, 0, 139999);
	assert(m.state == MON_PENDING);
	feed(&m, 100, 140000);
	assert(m.state == MON_RUNNING);
	for (int64_t t = 200; t < 30200; t += 100) { feed(&m, t, LOW_UW); }
	assert(m.state == MON_PENDING);
	feed(&m, 30200, LOW_UW);
	assert(m.state == MON_OFF);

	/* 待机期间按键重新计时，连续低功率时长从下一次采样重新起算。 */
	start(&m);
	for (int64_t t = 0; t <= 10000; t += 100) { feed(&m, t, LOW_UW); }
	assert(monitor_enable(&m, 10000));
	for (int64_t t = 10100; t < 40100; t += 100) { feed(&m, t, LOW_UW); }
	assert(m.state == MON_PENDING);
	feed(&m, 40100, LOW_UW);
	assert(m.state == MON_OFF);

	/* 通信异常立即断电；采集恢复后仍等待按键，防止自动重新上电。 */
	start(&m);
	monitor_sample(&m, 100, false, NULL);
	assert(m.state == MON_FAULT && !m.valid && !monitor_power_on(&m));
	assert(!monitor_enable(&m, 100));
	feed(&m, 200, RUN_UW);
	assert(m.state == MON_FAULT && m.valid && !monitor_power_on(&m));
	assert(monitor_enable(&m, 200));
	feed(&m, 300, RUN_UW);
	assert(m.state == MON_RUNNING);

	/* 500ms采样间隔允许通过，超过后关断；接通后的首个采样同样受约束。 */
	feed(&m, 800, RUN_UW);
	assert(m.state == MON_RUNNING);
	feed(&m, 1301, RUN_UW);
	assert(m.state == MON_FAULT && !monitor_power_on(&m));
	start(&m);
	feed(&m, 501, RUN_UW);
	assert(m.state == MON_FAULT);
	start(&m);
	feed(&m, 100, RUN_UW);
	feed(&m, 99, RUN_UW);
	assert(m.state == MON_FAULT);

	/* 空载、反向电流均走低功率计时，完整覆盖原始有符号读数。 */
	start(&m);
	for (int64_t t = 0; t <= 30000; t += 100) { feed(&m, t, 0); }
	assert(m.state == MON_OFF);
	start(&m);
	for (int64_t t = 0; t <= 30000; t += 100) { feed(&m, t, -LOW_UW); }
	assert(m.state == MON_OFF);
	puts("monitor: fixed loads, 30s cutoff, threshold, reset, recovery and faults passed");
}
