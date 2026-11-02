#include "ui_model.h"
#include "ui_render.h"
#include "button.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void save(const char *name, const struct ui_view *view)
{
	uint8_t frame[OLED_FRAME_BYTES];
	ui_render(view, frame);
	char path[128];
	snprintf(path, sizeof(path), "tests/ui-previews/%s.pbm", name);
	FILE *f = fopen(path, "w");
	assert(f);
	fprintf(f, "P1\n128 64\n");
	for (int y = 0; y < OLED_HEIGHT; ++y) {
		for (int x = 0; x < OLED_WIDTH; ++x) {
			/* PBM black=1; invert for white OLED text on a black preview. */
			fprintf(f, "%u ", !(frame[y/8*128+x] & (1U << (y%8))));
		}
		fputc('\n', f);
	}
	fclose(f);
}

static void button_test(void)
{
	struct button b = {0};
	assert(!button_update(&b, true, 0));
	assert(!button_update(&b, false, 10)); /* Bounce does not trigger an action. */
	assert(!button_update(&b, true, 15));
	assert(!button_update(&b, true, 45));
	assert(!button_update(&b, true, 2045)); /* Holding emits no repeated actions. */
	assert(!button_update(&b, false, 2050));
	assert(button_update(&b, false, 2080));
	assert(!button_update(&b, false, 3000));
}

int main(void)
{
	struct monitor m;
	struct ui_view view;
	uint64_t r;
	monitor_init(&m);
	ui_build(&m, 0, &view);
	assert(!strcmp(view.lines[0], "U:--V I:--A"));
	assert(!strcmp(view.lines[4], "已断电"));
	save("off", &view);
	struct measurement sample = {12000, 50000, 600000};
	monitor_sample(&m, 0, true, &sample);
	assert(monitor_enable(&m, 0));
	for (int64_t t=0; t<=4000; t+=100) {
		monitor_sample(&m, t, true, &sample);
	}
	assert(monitor_resistance_mohm(&m, &r) && r == 240000);
	ui_build(&m, 4000, &view);
	assert(!view.warning);
	assert(!strcmp(view.lines[0], "U:12.00V I:0.050A"));
	assert(!strcmp(view.lines[1], "P: 0.600 W"));
	assert(!strcmp(view.lines[2], "E: 0.0006 Wh"));
	assert(!strcmp(view.lines[3], "R: 240.0 Ohm"));
	save("running", &view);
	/* Zero, negative and sub-1mA currents cannot yield a useful resistance. */
	m.reading.current_ua = 0;
	assert(!monitor_resistance_mohm(&m, &r));
	m.reading.current_ua = -500000;
	assert(!monitor_resistance_mohm(&m, &r));
	m.reading.current_ua = 999;
	assert(!monitor_resistance_mohm(&m, &r));
	m.reading.current_ua = 1000;
	assert(monitor_resistance_mohm(&m, &r) && r == 12000000);
	/* Simulate 12V resistor below the power threshold; exercise the full UI flow. */
	sample = (struct measurement){12000, 20000, 240000};
	for (int64_t t=4100; t<=16100; t+=100) {
		monitor_sample(&m, t, true, &sample);
	}
	ui_build(&m, 16100, &view);
	assert(!strcmp(view.lines[4], "待机计时 12/30s"));
	save("pending", &view);
	for (int64_t t=16200; t<=34100; t+=100) {
		monitor_sample(&m, t, true, &sample);
	}
	ui_build(&m, 34100, &view);
	assert(view.warning && view.remaining_s == 5);
	save("warning", &view);
	ui_build(&m, 34101, &view);
	assert(view.remaining_s == 5); /* Round upwards for a useful countdown. */
	ui_build(&m, 38101, &view);
	assert(view.remaining_s == 1);
	assert(monitor_enable(&m, 34200)); /* PA30 cancels the reminder. */
	ui_build(&m, 34200, &view);
	assert(!view.warning && !strcmp(view.lines[4], "启动宽限 3s"));
	for (int64_t t=34200; t<=72200; t+=100) {
		monitor_sample(&m, t, true, &sample);
	}
	assert(m.state == MON_OFF);
	ui_build(&m, 72200, &view);
	assert(!view.warning && !strcmp(view.lines[3], "R: -- Ohm"));
	save("off", &view);
	assert(monitor_enable(&m, 72300));
	monitor_sample(&m, 72400, false, NULL);
	ui_build(&m, 72400, &view);
	assert(!strcmp(view.lines[0], "U:--V I:--A"));
	assert(!strcmp(view.lines[1], "P: -- W"));
	assert(!strcmp(view.lines[4], "传感器异常"));
	save("fault", &view);
	monitor_sample(&m, 72500, true, &sample);
	ui_build(&m, 72500, &view);
	assert(!strcmp(view.lines[4], "异常已解除"));
	assert(!monitor_power_on(&m));
	assert(monitor_enable(&m, 72600));
	sample = (struct measurement){12000, 170000, 2040000};
	monitor_sample(&m, 72700, true, &sample);
	assert(m.state == MON_OVERPOWER && !monitor_power_on(&m));
	ui_build(&m, 72700, &view);
	assert(!view.warning);
	assert(!strcmp(view.lines[4], "功率超限 已断电"));
	assert(!strcmp(view.lines[5], "按键恢复供电"));
	assert(!strcmp(view.lines[3], "R: -- Ohm"));
	save("overpower", &view);
	button_test();
	puts("ui: resistance, complete page flow, countdown cancellation and button debounce passed");
}
