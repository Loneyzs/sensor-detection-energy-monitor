#ifndef ENERGY_BUTTON_H
#define ENERGY_BUTTON_H
#include <stdbool.h>
#include <stdint.h>
struct button {
	bool raw;
	bool pressed;
	int64_t changed_ms;
};
/* pressed_level为GPIO低有效转换后的逻辑值；消抖后的松开沿仅产生一次动作。 */
bool button_update(struct button *button, bool pressed_level, int64_t now);
#endif
