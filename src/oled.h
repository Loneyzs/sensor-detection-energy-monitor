#ifndef ENERGY_OLED_H
#define ENERGY_OLED_H
#include "monitor.h"
void oled_start(const struct monitor *m);
void oled_publish(const struct monitor *m);
bool oled_is_ready(void);
#endif
