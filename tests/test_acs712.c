#include "acs712.h"
#include "monitor.h"
#include <zephyr/drivers/adc.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>

static int64_t now;
static bool ready;
static enum { OK, SETUP_FAIL, START_FAIL, COMPLETE_FAIL, TIMEOUT } mode;
static uint16_t current_raw = 1552, bus_raw = 1354;
static unsigned int requests;
int64_t k_uptime_get(void) { return now; }
void k_msleep(int ms) { now += ms; }
bool adc_is_ready_dt(const struct adc_dt_spec *s) { (void)s; return ready; }
int adc_channel_setup_dt(const struct adc_dt_spec *s)
{
	assert(s->channel_id == 2 || s->channel_id == 7);
	return mode == SETUP_FAIL ? -EIO : 0;
}
int adc_read_async(const void *dev, const struct adc_sequence *s, struct k_poll_signal *signal)
{
	(void)signal;
	assert(dev == (const void *)1);
	assert(s->channels == ((1U << 2) | (1U << 7)));
	assert(s->resolution == 12 && s->oversampling == 4);
	assert(s->options->interval_us == 1000 && s->options->extra_samplings == 31);
	assert(s->buffer_size == 32 * 2 * sizeof(uint16_t));
	requests++;
	if (mode == START_FAIL) { return -EIO; }
	uint16_t *out = s->buffer;
	for (unsigned int i = 0; i < 32; ++i) {
		*out++ = current_raw;
		*out++ = bus_raw;
	}
	return 0;
}
int k_poll(struct k_poll_event *e, int count, int timeout_ms)
{
	assert(count == 1 && (timeout_ms == 100 || timeout_ms == 0));
	if (mode == TIMEOUT) { now += timeout_ms; return -EAGAIN; }
	now += timeout_ms ? 32 : 0;
	e->signal->signaled = 1;
	e->signal->result = mode == COMPLETE_FAIL ? -EIO : 0;
	return 0;
}
int main(void)
{
	struct measurement s;
	assert(acs712_acquire(&s) == -ENODEV);
	ready = true;
	mode = SETUP_FAIL;
	assert(acs712_acquire(&s) == -EIO);
	mode = OK;
	/* 首次标定遇到断线/低电平时拒绝建立零点。 */
	current_raw = 0;
	assert(acs712_acquire(&s) == -ERANGE);
	assert(!acs712_diagnostics_get()->calibrated);
	current_raw = 1000; /* 未贴电源轨，但还原OUT不足2V，仍不能作为零点。 */
	assert(acs712_acquire(&s) == -ERANGE);
	assert(!acs712_diagnostics_get()->calibrated);
	current_raw = 1552;
	assert(acs712_acquire(&s) == 0);
	assert(acs712_diagnostics_get()->calibrated);
	assert(s.current_ua == 0 && s.power_uw == 0);
	assert(s.bus_mv >= 11990 && s.bus_mv <= 12010);
	int32_t zero_uv = acs712_diagnostics_get()->zero_uv;
	assert(zero_uv > 2490000 && zero_uv < 2510000);

	/* 12V、5Ω约2.4A，10Ω约1.2A，使用独立ADC原始码验证换算。 */
	current_raw = 1828;
	assert(acs712_acquire(&s) == 0);
	assert(s.current_ua > 2390000 && s.current_ua < 2410000);
	assert(s.power_uw > 28700000 && s.power_uw < 28900000);
	struct monitor m;
	monitor_init(&m);
	monitor_sample(&m, now, true, &s);
	assert(monitor_enable(&m, now));
	monitor_sample(&m, now, true, &s);
	assert(m.state == MON_RUNNING);
	current_raw = 1690;
	assert(acs712_acquire(&s) == 0);
	assert(s.current_ua > 1190000 && s.current_ua < 1210000);
	assert(s.power_uw > 14300000 && s.power_uw < 14500000);
	int64_t low_start = now;
	for (int64_t t = low_start; t <= low_start + 30000; t += 100) {
		monitor_sample(&m, t, true, &s);
	}
	assert(m.state == MON_OFF);
	assert(acs712_diagnostics_get()->zero_uv == zero_uv);
	current_raw = 1414;
	assert(acs712_acquire(&s) == 0 && s.current_ua < -1190000);
	current_raw = 1556;
	assert(acs712_acquire(&s) == 0 && s.current_ua == 0);

	/* 量程、母线电压及轨边异常应返回错误，供主循环关断输出。 */
	current_raw = 2152;
	assert(acs712_acquire(&s) == -ERANGE);
	current_raw = 1552;
	bus_raw = 1800;
	assert(acs712_acquire(&s) == -ERANGE);
	bus_raw = 1354;
	current_raw = 4095;
	assert(acs712_acquire(&s) == -ERANGE);
	current_raw = 1552;
	mode = START_FAIL;
	assert(acs712_acquire(&s) == -EIO);
	mode = COMPLETE_FAIL;
	assert(acs712_acquire(&s) == -EIO);

	/* 超时期间禁止重用请求；迟到的请求完成后丢弃旧值、重新采样恢复。 */
	mode = TIMEOUT;
	int64_t before = now;
	assert(acs712_acquire(&s) == -ETIMEDOUT && now - before == 100);
	unsigned int pending_requests = requests;
	assert(acs712_acquire(&s) == -ETIMEDOUT && requests == pending_requests);
	mode = OK;
	assert(acs712_acquire(&s) == 0 && requests == pending_requests + 1);
	assert(acs712_diagnostics_get()->zero_uv == zero_uv);
	puts("acs712: zero calibration, ADC scaling, fixed-load integration, faults and timeout recovery passed");
}
