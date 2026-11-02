#ifndef TEST_I2C_H
#define TEST_I2C_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define DT_NODELABEL(name) name
#define TEST_ADDR_oled 0x3c
#define TEST_ADDR_ina219 0x40
#define TEST_JOIN_INNER(a, b) a##b
#define TEST_JOIN(a, b) TEST_JOIN_INNER(a, b)
#define DT_REG_ADDR(node) TEST_JOIN(TEST_ADDR_, node)
#define DT_PROP(node, prop) 100000
struct i2c_dt_spec { uint16_t addr; };
#define I2C_DT_SPEC_GET(node) { .addr = DT_REG_ADDR(node) }
bool i2c_is_ready_dt(const struct i2c_dt_spec *bus);
int i2c_write_dt(const struct i2c_dt_spec *bus, const uint8_t *data, size_t size);
int i2c_write_read_dt(const struct i2c_dt_spec *bus, const void *tx, size_t tx_size, void *rx, size_t rx_size);
#endif
