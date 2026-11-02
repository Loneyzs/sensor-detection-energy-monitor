#include "ui_render.h"
#include "font.h"
#include <string.h>

static void pixel(uint8_t *frame, int x, int y)
{
	if (x >= 0 && x < OLED_WIDTH && y >= 0 && y < OLED_HEIGHT) {
		frame[(y / 8) * OLED_WIDTH + x] |= 1U << (y % 8);
	}
}

static void text(uint8_t *frame, int x, int y, const char *str, int scale)
{
	while (*str && x < OLED_WIDTH) {
		uint32_t cp = (uint8_t)*str++;
		if ((cp & 0xf0) == 0xe0) {
			cp = (cp & 15) << 12;
			cp |= ((uint8_t)*str++ & 63) << 6;
			cp |= (uint8_t)*str++ & 63;
		}
		const struct font_glyph *glyph = NULL;
		for (size_t i = 0; i < sizeof(font_glyphs) / sizeof(font_glyphs[0]); ++i) {
			if (font_glyphs[i].codepoint == cp) {
				glyph = &font_glyphs[i];
				break;
			}
		}
		if (!glyph) {
			x += 6 * scale;
			continue;
		}
		for (int row = 0; row < glyph->height; ++row) {
			for (int col = 0; col < glyph->width; ++col) {
				if (!(glyph->rows[row] & (1U << col))) {
					continue;
				}
				for (int dy = 0; dy < scale; ++dy) {
					for (int dx = 0; dx < scale; ++dx) {
						pixel(frame, x + col * scale + dx, y + row * scale + dy);
					}
				}
			}
		}
		x += glyph->width * scale;
	}
}

void ui_render(const struct ui_view *view, uint8_t frame[OLED_FRAME_BYTES])
{
	memset(frame, 0, OLED_FRAME_BYTES);
	if (view->warning) {
		text(frame, 40, 0, "即将断电", 1);
		char digits[12];
		/* Countdown is normally 1..5; 0 is shown until the next acquisition switches off. */
		unsigned int n = view->remaining_s;
		int len = 0;
		do { digits[len++] = '0' + n % 10; n /= 10; } while (n && len < 10);
		for (int i = 0; i < len / 2; ++i) {
			char c = digits[i]; digits[i] = digits[len-1-i]; digits[len-1-i] = c;
		}
		digits[len] = '\0';
		text(frame, (OLED_WIDTH - len * 18 - 12) / 2, 20, digits, 3);
		text(frame, (OLED_WIDTH + len * 18 - 12) / 2, 28, "秒", 1);
		text(frame, 28, 52, "按键取消断电", 1);
		return;
	}
	static const int y[] = {0, 10, 20, 30, 40, 52};
	for (int i = 0; i < UI_LINE_COUNT; ++i) {
		text(frame, 0, y[i], view->lines[i], 1);
	}
}
