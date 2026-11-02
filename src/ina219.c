#include "ina219.h"
#include <errno.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>

#define INA DT_NODELABEL(ina219)
#define SHUNT_UOHM DT_PROP(INA, shunt_micro_ohms)
BUILD_ASSERT(SHUNT_UOHM > 0, "Shunt resistance must be positive");
static const struct i2c_dt_spec bus = I2C_DT_SPEC_GET(INA);
static struct ina219_diagnostics diagnostics;

const struct ina219_diagnostics *ina219_diagnostics_get(void)
{
	return &diagnostics;
}

static int failed(enum ina219_step step, int error)
{
	diagnostics.failures[step]++;
	diagnostics.last_failure = step;
	diagnostics.last_error = error;
	return error;
}

static int read_reg(uint8_t reg, uint16_t *value)
{
	uint8_t bytes[2];
	int rc = i2c_write_read_dt(&bus, &reg, 1, bytes, sizeof(bytes));
	if (!rc) {
		*value = sys_get_be16(bytes);
	}
	return rc;
}

int ina219_acquire(struct measurement *sample)
{
	diagnostics.attempts++;
	if (!i2c_is_ready_dt(&bus)) {
		return failed(INA_CONFIG, -ENODEV);
	}
	/* 32V register range (physical input <=26V), +/-320mV shunt,
	 * 16-sample averaging on each ADC, shunt+bus continuous mode. This module needs continuous mode
	 * to keep responding reliably between acquisitions.
	 * Read raw shunt: current/power calibration registers are not used.
	 */
	const uint16_t config = (1U << 13) | (3U << 11) | (12U << 7) | (12U << 3) | 7U;
	uint8_t tx[] = {0, config >> 8, config & 0xff};
	int rc = i2c_write_dt(&bus, tx, sizeof(tx));
	if (rc) {
		return failed(INA_CONFIG, rc);
	}
	int64_t deadline = k_uptime_get() + 50;
	uint16_t status = 0;
	do {
		k_msleep(2);
		rc = read_reg(2, &status);
		if (rc) {
			return failed(INA_BUS, rc);
		}
		if (status & 2) {
			break;
		}
	} while (k_uptime_get() < deadline);
	if (!(status & 2)) {
		return failed(INA_CONVERSION, -ETIMEDOUT);
	}
	uint16_t shunt;
	rc = read_reg(1, &shunt);
	if (rc) {
		return failed(INA_SHUNT, rc);
	}
	diagnostics.bus_raw = status;
	diagnostics.shunt_raw = shunt;
	int32_t shunt_uv = (int16_t)shunt * 10;
	int32_t bus_mv = (status >> 3) * 4;
	if (bus_mv > 26000 || shunt_uv >= 320000 || shunt_uv <= -320000) {
		return failed(INA_RANGE, -ERANGE);
	}
	/* Signed reverse current is retained; consumption integration uses positive power. */
	sample->bus_mv = bus_mv;
	sample->current_ua = (int64_t)shunt_uv * 1000000 / SHUNT_UOHM;
	sample->power_uw = (int64_t)bus_mv * sample->current_ua / 1000;
	diagnostics.successes++;
	return 0;
}

#ifdef CONFIG_DEBUG_OPTIMIZATIONS
#include <zephyr/sys/printk.h>
volatile uint32_t ina219_debug_scan_request;
void ina219_debug_scan(void)
{
	for (uint16_t address = 0x40; address <= 0x4f; ++address) {
		struct i2c_dt_spec target = bus;
		target.addr = address;
		uint8_t reg = 0, bytes[2] = {0};
		int rc = i2c_write_read_dt(&target, &reg, 1, bytes, 2);
		printk("INA_SCAN addr=0x%02x rc=%d config=%04x\n",
		       address, rc, sys_get_be16(bytes));
	}
}
#endif
