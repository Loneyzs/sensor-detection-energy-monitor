#include "button.h"

bool button_update(struct button *button, bool high, int64_t now)
{
	if (high != button->raw) {
		button->raw = high;
		button->changed_ms = now;
	}
	if (button->pressed != button->raw && now - button->changed_ms >= 30) {
		button->pressed = button->raw;
		return !button->pressed;
	}
	return false;
}
