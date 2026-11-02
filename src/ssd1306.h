#ifndef ENERGY_SSD1306_H
#define ENERGY_SSD1306_H
#include "ui_render.h"
int ssd1306_init(void);
int ssd1306_set_display(bool on);
int ssd1306_update(const uint8_t frame[OLED_FRAME_BYTES]);
#endif
