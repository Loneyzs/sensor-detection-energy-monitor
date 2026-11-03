#include "button.h"

bool button_update(struct button *button, bool pressed_level, int64_t now)
{
	if (pressed_level != button->raw) {
		button->raw = pressed_level;
		button->changed_ms = now;
	}
	/* 电平连续稳定30ms才确认，长按期间保持供电状态，松开时执行恢复。 */
	if (button->pressed != button->raw && now - button->changed_ms >= 30) {
		button->pressed = button->raw;
		return !button->pressed;
	}
	return false;
}
