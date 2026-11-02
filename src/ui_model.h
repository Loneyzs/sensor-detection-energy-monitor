#ifndef ENERGY_UI_MODEL_H
#define ENERGY_UI_MODEL_H
#include "monitor.h"
#define UI_LINE_COUNT 6
#define UI_LINE_BYTES 64
struct ui_view {
	bool warning;
	uint32_t remaining_s;
	char lines[UI_LINE_COUNT][UI_LINE_BYTES];
};
bool monitor_resistance_mohm(const struct monitor *m, uint64_t *value);
void ui_build(const struct monitor *m, int64_t now, struct ui_view *view);
#endif
