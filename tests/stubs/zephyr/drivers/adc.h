#ifndef TEST_ADC_H
#define TEST_ADC_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#define DT_NODELABEL(name) name
#define TEST_PROP_sensitivity_microvolt_per_amp 185000
#define TEST_PROP_current_divider_top_ohms 10000
#define TEST_PROP_current_divider_bottom_ohms 10000
#define TEST_PROP_bus_divider_top_ohms 10000
#define TEST_PROP_bus_divider_bottom_ohms 1000
#define TEST_PROP_INNER(p) TEST_PROP_##p
#define DT_PROP(n, p) TEST_PROP_INNER(p)
#define DT_IO_CHANNELS_INPUT_BY_IDX(n, i) ((i) == 0 ? 2 : 7)
struct adc_dt_spec { const void *dev; uint8_t channel_id; };
#define ADC_DT_SPEC_GET_BY_IDX(n, i) { .dev = (const void *)1, .channel_id = ((i) == 0 ? 2 : 7) }
struct adc_sequence_options { uint32_t interval_us; uint16_t extra_samplings; };
struct adc_sequence {
	uint32_t channels;
	void *buffer;
	size_t buffer_size;
	uint8_t resolution, oversampling;
	const struct adc_sequence_options *options;
};
bool adc_is_ready_dt(const struct adc_dt_spec *spec);
int adc_channel_setup_dt(const struct adc_dt_spec *spec);
int adc_read_async(const void *dev, const struct adc_sequence *sequence, struct k_poll_signal *signal);
#endif
