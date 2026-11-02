#ifndef ENERGY_BUTTON_H
#define ENERGY_BUTTON_H
#include <stdbool.h>
#include <stdint.h>
struct button {
	bool raw;
	bool pressed;
	int64_t changed_ms;
};
/* A release after a debounced press emits one action, regardless of hold time. */
bool button_update(struct button *button, bool high, int64_t now);
#endif
