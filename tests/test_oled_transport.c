#include "ssd1306.h"
#include <zephyr/drivers/i2c.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static bool ready = true;
static uint16_t address = 0x3d;
static unsigned int writes, column, page;
static int fail_after = -1;
static bool enabled;
static uint8_t ram[OLED_FRAME_BYTES];

bool i2c_is_ready_dt(const struct i2c_dt_spec *bus)
{
	(void)bus;
	return ready;
}

int i2c_write_dt(const struct i2c_dt_spec *bus, const uint8_t *data, size_t size)
{
	writes++;
	if (bus->addr != address || fail_after == 0) {
		return -EIO;
	}
	if (fail_after > 0) { fail_after--; }
	assert(size >= 2 && size <= 33);
	if (data[0] == 0x40) {
		assert(page < 8 && column + size - 1 <= 128);
		memcpy(ram + page * 128 + column, data + 1, size - 1);
		column += size - 1;
	} else {
		assert(data[0] == 0x00);
		if (data[1] == 0x21) {
			assert(size == 7 && data[2] == 0 && data[3] == 127);
			assert(data[4] == 0x22 && data[5] == data[6]);
			column = 0; page = data[5];
		} else if (data[1] == 0xaf) {
			assert(size == 2);
			enabled = true;
		} else {
			assert(data[1] == 0xae); /* initialize with display off */
			enabled = false;
		}
	}
	return 0;
}

int main(void)
{
	uint8_t frame[OLED_FRAME_BYTES];
	for (unsigned int i = 0; i < sizeof(frame); ++i) { frame[i] = i * 37; }
	assert(ssd1306_init() == 0 && writes == 2); /* fallback from 0x3c to 0x3d */
	assert(!enabled);
	assert(ssd1306_update(frame) == 0 && enabled);
	assert(!memcmp(ram, frame, sizeof(frame)));
	assert(ssd1306_set_display(false) == 0 && !enabled);
	assert(ssd1306_set_display(true) == 0 && enabled);
	unsigned int before = writes;
	assert(ssd1306_update(frame) == 0 && writes == before); /* unchanged frame */
	frame[400] ^= 0xff;
	assert(ssd1306_update(frame) == 0 && writes == before + 5); /* one page */
	assert(!memcmp(ram, frame, sizeof(frame)));
	frame[10] ^= 0xff;
	fail_after = 2;
	assert(ssd1306_update(frame) == -EIO); /* failure midway through one page */
	fail_after = -1;
	assert(ssd1306_init() == 0);
	assert(ssd1306_update(frame) == 0);
	assert(!memcmp(ram, frame, sizeof(frame))); /* full repaint after reconnection */
	ready = false;
	assert(ssd1306_init() == -ENODEV);
	ready = true; address = 0x42;
	before = writes;
	assert(ssd1306_init() == -EIO && writes == before + 2);
	puts("oled: address discovery, page transfers, dirty pages and failure recovery passed");
}
