#ifndef ENERGY_UI_MODEL_H
#define ENERGY_UI_MODEL_H
#include "monitor.h"
#define UI_LINE_COUNT 6
#define UI_LINE_BYTES 64
struct ui_view {
	char lines[UI_LINE_COUNT][UI_LINE_BYTES];
};
void ui_build(const struct monitor *m, int64_t now, struct ui_view *view);
#endif
