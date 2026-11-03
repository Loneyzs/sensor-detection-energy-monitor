#include "ina219.h"
#include <zephyr/drivers/i2c.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static int64_t now;
static bool bus_ready = true;
static enum { OK, CFG_FAIL, BUS_FAIL, SHUNT_FAIL, TIMEOUT, HIGH_V, HIGH_I, REVERSE } mode;
static int64_t conversion_start;
static int16_t raw_shunt = 1000;
int64_t k_uptime_get(void) { return now; }
void k_msleep(int ms) { now += ms; }
bool i2c_is_ready_dt(const struct i2c_dt_spec *bus) { (void)bus; return bus_ready; }
int i2c_write_dt(const struct i2c_dt_spec *bus, const uint8_t *data, size_t size)
{
	assert(bus->addr == 0x40 && size == 3 && data[0] == 0);
	assert(data[1] == 0x3e && data[2] == 0x67);
	conversion_start = now;
	return mode == CFG_FAIL ? -EIO : 0;
}
int i2c_write_read_dt(const struct i2c_dt_spec *bus, const void *tx, size_t tx_size,
		      void *rx, size_t rx_size)
{
	assert(bus->addr == 0x40 && tx_size == 1 && rx_size == 2);
	uint8_t reg = *(const uint8_t *)tx;
	if ((reg == 2 && mode == BUS_FAIL) || (reg == 1 && mode == SHUNT_FAIL)) {
		return -EIO;
	}
	uint16_t value;
	if (reg == 2) {
		value = ((mode == HIGH_V ? 27000 : 12000) / 4) << 3;
		if (mode != TIMEOUT && now - conversion_start >= 18) { value |= 2; }
	} else {
		assert(reg == 1);
		value = mode == REVERSE ? (uint16_t)-1000 : mode == HIGH_I ? 32767 : (uint16_t)raw_shunt;
	}
	uint8_t *out = rx;
	out[0] = value >> 8; out[1] = value;
	return 0;
}
int main(void)
{
	struct measurement sample;
	assert(ina219_acquire(&sample) == 0);
	assert(sample.bus_mv == 12000 && sample.current_ua == 100000 && sample.power_uw == 1200000);
	bus_ready = false;
	assert(ina219_acquire(&sample) == -ENODEV);
	bus_ready = true;
	mode = CFG_FAIL; assert(ina219_acquire(&sample) == -EIO);
	mode = BUS_FAIL; assert(ina219_acquire(&sample) == -EIO);
	mode = SHUNT_FAIL; assert(ina219_acquire(&sample) == -EIO);
	mode = TIMEOUT;
	int64_t before = now;
	assert(ina219_acquire(&sample) == -ETIMEDOUT && now - before == 50);
	mode = HIGH_V; assert(ina219_acquire(&sample) == -ERANGE);
	mode = HIGH_I; assert(ina219_acquire(&sample) == -ERANGE);
	mode = REVERSE;
	assert(ina219_acquire(&sample) == 0 && sample.current_ua == -100000 && sample.power_uw == -1200000);
	/* 新负载的真实寄存器数量级：R100每计数对应0.1mA。 */
	mode = OK;
	raw_shunt = 176;
	assert(ina219_acquire(&sample) == 0);
	assert(sample.current_ua == 17600 && sample.power_uw == 211200);
	raw_shunt = 60;
	assert(ina219_acquire(&sample) == 0);
	assert(sample.current_ua == 6000 && sample.power_uw == 72000);
	raw_shunt = 0;
	assert(ina219_acquire(&sample) == 0);
	assert(sample.current_ua == 0 && sample.power_uw == 0);
	const struct ina219_diagnostics *d = ina219_diagnostics_get();
	assert(d->attempts == 12 && d->successes == 5);
	assert(d->failures[INA_CONFIG] == 2 && d->failures[INA_BUS] == 1);
	assert(d->failures[INA_SHUNT] == 1 && d->failures[INA_CONVERSION] == 1 && d->failures[INA_RANGE] == 2);
	puts("ina219: conversion, error stages, timeout, signed current and ranges passed");
}
