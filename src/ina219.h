#ifndef ENERGY_INA219_H
#define ENERGY_INA219_H
#include "monitor.h"
enum ina219_step { INA_CONFIG, INA_BUS, INA_SHUNT, INA_CONVERSION, INA_RANGE, INA_STEPS };
struct ina219_diagnostics {
	uint32_t attempts, successes;
	uint32_t failures[INA_STEPS];
	int last_error;
	enum ina219_step last_failure;
	uint16_t bus_raw, shunt_raw;
};
int ina219_acquire(struct measurement *sample);
const struct ina219_diagnostics *ina219_diagnostics_get(void);
#ifdef CONFIG_DEBUG_OPTIMIZATIONS
extern volatile uint32_t ina219_debug_scan_request;
void ina219_debug_scan(void);
#endif
#endif
