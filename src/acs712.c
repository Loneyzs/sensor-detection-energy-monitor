#include "acs712.h"
#include "app_config.h"
#include <errno.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>

#define SENSOR DT_NODELABEL(acs712)
#define SENSITIVITY_UV_A DT_PROP(SENSOR, sensitivity_microvolt_per_amp)
#define CURRENT_DIV_TOP DT_PROP(SENSOR, current_divider_top_ohms)
#define CURRENT_DIV_BOTTOM DT_PROP(SENSOR, current_divider_bottom_ohms)
#define BUS_DIV_TOP DT_PROP(SENSOR, bus_divider_top_ohms)
#define BUS_DIV_BOTTOM DT_PROP(SENSOR, bus_divider_bottom_ohms)
#define SAMPLE_COUNT 32
#define ZERO_BATCHES 8
#define ADC_COUNTS 4096

static const struct adc_dt_spec current_adc = ADC_DT_SPEC_GET_BY_IDX(SENSOR, 0);
static const struct adc_dt_spec bus_adc = ADC_DT_SPEC_GET_BY_IDX(SENSOR, 1);
/* 按通道号从小到大排列：ADC0_CH2电流、ADC0_CH7母线电压。 */
BUILD_ASSERT(DT_IO_CHANNELS_INPUT_BY_IDX(SENSOR, 0) <
	     DT_IO_CHANNELS_INPUT_BY_IDX(SENSOR, 1), "ADC channels must be ordered");
BUILD_ASSERT(CURRENT_DIV_BOTTOM > 0 && BUS_DIV_BOTTOM > 0 && SENSITIVITY_UV_A > 0,
	     "ADC divider and sensitivity must be positive");
static uint16_t samples[SAMPLE_COUNT][2];
static struct k_poll_signal completion;
static struct k_poll_event event;
static bool initialized, in_flight;
static struct acs712_diagnostics diagnostics;

const struct acs712_diagnostics *acs712_diagnostics_get(void)
{
	return &diagnostics;
}

static int adc_init(void)
{
	if (!adc_is_ready_dt(&current_adc) || !adc_is_ready_dt(&bus_adc)) {
		return -ENODEV;
	}
	int rc = adc_channel_setup_dt(&current_adc);
	if (!rc) {
		rc = adc_channel_setup_dt(&bus_adc);
	}
	if (rc) {
		return rc;
	}
	k_poll_signal_init(&completion);
	k_poll_event_init(&event, K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &completion);
	initialized = true;
	/* 外接10kΩ/10kΩ分压、ADC端100nF滤波，等待电压建立后再标定零点。 */
	k_msleep(10);
	return 0;
}

static int finish_read(int timeout_ms)
{
	int rc = k_poll(&event, 1, K_MSEC(timeout_ms));
	if (rc) {
		return rc == -EAGAIN ? -ETIMEDOUT : rc;
	}
	unsigned int signaled;
	int result;
	k_poll_signal_check(&completion, &signaled, &result);
	if (!signaled) {
		return -EIO;
	}
	in_flight = false;
	return result;
}

static int read_average(int32_t *current_uv, int32_t *bus_uv)
{
	/* 超时后仍保留静态缓冲区，等旧请求完成再发新请求，避免覆盖未完成的ADC操作。 */
	if (in_flight) {
		int rc = finish_read(0);
		if (in_flight) {
			return rc;
		}
	}
	k_poll_signal_reset(&completion);
	event.state = K_POLL_STATE_NOT_READY;
	const struct adc_sequence_options options = {
		.interval_us = 1000,
		.extra_samplings = SAMPLE_COUNT - 1,
	};
	const struct adc_sequence sequence = {
		.channels = (1U << current_adc.channel_id) | (1U << bus_adc.channel_id),
		.buffer = samples,
		.buffer_size = sizeof(samples),
		.resolution = 12,
		.oversampling = 4, /* 每次硬件16次平均，再在约32ms内对32组结果做软件平均。 */
		.options = &options,
	};
	int rc = adc_read_async(current_adc.dev, &sequence, &completion);
	if (rc) {
		return rc;
	}
	in_flight = true;
	rc = finish_read(100);
	if (rc) {
		return rc;
	}
	uint32_t current_sum = 0, bus_sum = 0;
	for (unsigned int i = 0; i < SAMPLE_COUNT; ++i) {
		/* ADC贴近电源轨时拒绝读数，检查断线、短接或输入电压异常。 */
		if (samples[i][0] < 16 || samples[i][0] > 4079 ||
		    samples[i][1] < 16 || samples[i][1] > 4079) {
			return -ERANGE;
		}
		current_sum += samples[i][0];
		bus_sum += samples[i][1];
	}
	diagnostics.current_raw = (current_sum + SAMPLE_COUNT / 2) / SAMPLE_COUNT;
	diagnostics.bus_raw = (bus_sum + SAMPLE_COUNT / 2) / SAMPLE_COUNT;
	/* ADC以VDDA为参考，12位码值乘实测参考电压/4096得到引脚电压µV。
	 * 先累加原始值，再统一换算，保留平均后的亚LSB信息。
	 * ADC_REFERENCE_UV默认3300000，实物调试可按万用表读数修正。
	 */
	*current_uv = ((int64_t)current_sum * ADC_REFERENCE_UV +
		      (int64_t)SAMPLE_COUNT * ADC_COUNTS / 2) / (SAMPLE_COUNT * ADC_COUNTS);
	*bus_uv = ((int64_t)bus_sum * ADC_REFERENCE_UV +
		  (int64_t)SAMPLE_COUNT * ADC_COUNTS / 2) / (SAMPLE_COUNT * ADC_COUNTS);
	return 0;
}

static int acquire(struct measurement *sample)
{
	int rc;
	if (!initialized && (rc = adc_init())) {
		return rc;
	}
	if (!diagnostics.calibrated) {
		/* 上电PB14保持低，负载电流为零。8×32组平均标定实际零点，约0.3秒。
		 * 05B在5V供电时零点约2.5V；OUT经过1:1分压后ADC端约1.25V。
		 * 标定仅在启动时进行，运行中停机也保留零点，避免把带载值误当作零。
		 */
		int64_t zero_sum = 0;
		for (unsigned int i = 0; i < ZERO_BATCHES; ++i) {
			int32_t current_uv, bus_uv;
			rc = read_average(&current_uv, &bus_uv);
			if (rc) {
				return rc;
			}
			int32_t output_uv = (int64_t)current_uv *
				(CURRENT_DIV_TOP + CURRENT_DIV_BOTTOM) / CURRENT_DIV_BOTTOM;
			if (output_uv < 2000000 || output_uv > 3000000) {
				return -ERANGE;
			}
			zero_sum += output_uv;
		}
		diagnostics.zero_uv = (zero_sum + ZERO_BATCHES / 2) / ZERO_BATCHES;
		diagnostics.calibrated = true;
	}
	int32_t current_uv, bus_uv;
	rc = read_average(&current_uv, &bus_uv);
	if (rc) {
		return rc;
	}
	/* 还原分压前电压：OUT分压10k/10k乘2；底板R18=10k、R19=1k乘11。
	 * PA22测的是VCCBAT输入总线，与负载使用同一供电节点。
	 */
	diagnostics.output_uv = (int64_t)current_uv *
		(CURRENT_DIV_TOP + CURRENT_DIV_BOTTOM) / CURRENT_DIV_BOTTOM;
	sample->bus_mv = ((int64_t)bus_uv * (BUS_DIV_TOP + BUS_DIV_BOTTOM) /
			  BUS_DIV_BOTTOM + 500) / 1000;
	/* ACS712-05B：灵敏度185mV/A=185000µV/A（5V供电标称值）。
	 * I[µA]=(OUT[µV]-零点[µV])×1000000/185000。
	 * 例如2.944V-2.500V=0.444V，对应2.4A；2.722V对应1.2A。
	 * 保留负号便于发现IP+/IP-接反；50mA内归零以抑制零点噪声。
	 * 全部乘法先转64位，防止微单位换算溢出。
	 */
	sample->current_ua = (int64_t)(diagnostics.output_uv - diagnostics.zero_uv) *
		1000000 / SENSITIVITY_UV_A;
	if (sample->bus_mv < 8000 || sample->bus_mv > 15000 ||
	    sample->current_ua < -5000000 || sample->current_ua > 5000000) {
		return -ERANGE;
	}
	if (sample->current_ua > -ACS712_ZERO_DEADBAND_UA &&
	    sample->current_ua < ACS712_ZERO_DEADBAND_UA) {
		sample->current_ua = 0;
	}
	/* P[µW]=V[mV]×I[µA]/1000，使用实测输入电压计算功率和界面等效阻值。 */
	sample->power_uw = (int64_t)sample->bus_mv * sample->current_ua / 1000;
	return 0;
}

int acs712_acquire(struct measurement *sample)
{
	diagnostics.attempts++;
	int rc = acquire(sample);
	if (rc) {
		diagnostics.failures++;
		diagnostics.last_error = rc;
	} else {
		diagnostics.successes++;
	}
	return rc;
}
