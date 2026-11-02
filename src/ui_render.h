#ifndef ENERGY_UI_RENDER_H
#define ENERGY_UI_RENDER_H
#include "ui_model.h"
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_FRAME_BYTES (OLED_WIDTH * OLED_HEIGHT / 8)
void ui_render(const struct ui_view *view, uint8_t frame[OLED_FRAME_BYTES]);
#endif
