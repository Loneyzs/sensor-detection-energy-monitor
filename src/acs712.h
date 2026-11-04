#ifndef ENERGY_ACS712_H
#define ENERGY_ACS712_H
#include "monitor.h"

/* 调用前先将MOSFET关闭；首次采集自动在零电流下标定，之后沿用该零点。 */
int acs712_acquire(struct measurement *sample);
struct acs712_diagnostics {
	uint32_t attempts, successes, failures;
	int last_error;
	uint16_t current_raw, bus_raw;
	int32_t output_uv, zero_uv;
	bool calibrated;
};
const struct acs712_diagnostics *acs712_diagnostics_get(void);
#endif
