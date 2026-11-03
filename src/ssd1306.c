#include "ssd1306.h"
#include <errno.h>
#include <string.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>

#define OLED DT_NODELABEL(oled)
static struct i2c_dt_spec bus = I2C_DT_SPEC_GET(OLED);
static uint8_t previous[OLED_FRAME_BYTES];
static bool first_frame;

int ssd1306_init(void)
{
	if (!i2c_is_ready_dt(&bus)) {
		return -ENODEV;
	}
	/* SSD1306 128×64使用内部电荷泵，0x00选择命令流。
	 * 先保持关屏，首帧完整传输后再点亮，避免显示未初始化的显存。
	 */
	static const uint8_t init[] = {
		0x00, 0xae, 0xd5, 0x80, 0xa8, 0x3f, 0xd3, 0x00, 0x40,
		0x8d, 0x14, 0x20, 0x00, 0xa1, 0xc8, 0xda, 0x12,
		0x81, 0x7f, 0xd9, 0xf1, 0xdb, 0x40, 0xa4, 0xa6, 0x2e
	};
	const uint16_t addresses[] = {DT_REG_ADDR(OLED), DT_REG_ADDR(OLED) ^ 1U};
	int rc = -ENODEV;
	for (size_t i = 0; i < ARRAY_SIZE(addresses); ++i) {
		bus.addr = addresses[i];
		rc = i2c_write_dt(&bus, init, sizeof(init));
		if (!rc) {
			first_frame = true;
			return 0;
		}
	}
	return rc;
}

int ssd1306_update(const uint8_t frame[OLED_FRAME_BYTES])
{
	for (unsigned int page = 0; page < 8; ++page) {
		unsigned int offset = page * OLED_WIDTH;
		if (!first_frame && !memcmp(previous + offset, frame + offset, OLED_WIDTH)) {
			continue;
		}
		const uint8_t window[] = {0x00, 0x21, 0x00, 0x7f, 0x22, page, page};
		int rc = i2c_write_dt(&bus, window, sizeof(window));
		if (rc) {
			return rc;
		}
		/* 每次发送32字节并主动让出CPU，给采样控制线程运行机会。 */
		for (unsigned int col = 0; col < OLED_WIDTH; col += 32) {
			uint8_t data[33] = {0x40};
			memcpy(data + 1, frame + offset + col, 32);
			rc = i2c_write_dt(&bus, data, sizeof(data));
			if (rc) {
				return rc;
			}
			k_yield();
		}
		memcpy(previous + offset, frame + offset, OLED_WIDTH);
	}
	if (first_frame) {
		const uint8_t on[] = {0x00, 0xaf};
		int rc = i2c_write_dt(&bus, on, sizeof(on));
		if (rc) {
			return rc;
		}
		first_frame = false;
	}
	return 0;
}

int ssd1306_set_display(bool on)
{
	const uint8_t command[] = {0x00, on ? 0xaf : 0xae};
	return i2c_write_dt(&bus, command, sizeof(command));
}
