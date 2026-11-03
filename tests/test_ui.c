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
	monitor_init(&m);
	ui_build(&m, 0, &view);
	assert(!strcmp(view.lines[0], "V: -- V"));
	assert(!strcmp(view.lines[1], "I: -- mA"));
	assert(!strcmp(view.lines[2], "P: -- W"));
	assert(!strcmp(view.lines[3], "已断电"));
	assert(!strcmp(view.lines[4], "按键恢复供电"));

	/* 680Ω正常档：电流用mA显示，保留INA219原始分辨率0.1mA。 */
	struct measurement sample = {12000, 17600, 211200};
	monitor_sample(&m, 0, true, &sample);
	assert(monitor_enable(&m, 0));
	monitor_sample(&m, 0, true, &sample);
	ui_build(&m, 0, &view);
	assert(!strcmp(view.lines[0], "V: 12.00 V"));
	assert(!strcmp(view.lines[1], "I: 17.6 mA"));
	assert(!strcmp(view.lines[2], "P: 0.211 W"));
	assert(!strcmp(view.lines[3], "正常供电"));
	assert(view.lines[4][0] == '\0');
	save("running", &view);

	/* 切换2kΩ后在同一页面保留测量值及中文计时提示。 */
	sample = (struct measurement){12000, 6000, 72000};
	for (int64_t t = 100; t <= 12100; t += 100) {
		monitor_sample(&m, t, true, &sample);
	}
	ui_build(&m, 12100, &view);
	assert(!strcmp(view.lines[1], "I: 6.0 mA"));
	assert(!strcmp(view.lines[2], "P: 0.072 W"));
	assert(!strcmp(view.lines[3], "待机计时 12/30秒"));
	assert(!strcmp(view.lines[4], "按键重新计时"));
	save("pending", &view);

	struct measurement normal = {12000, 17600, 211200};
	monitor_sample(&m, 12200, true, &normal);
	ui_build(&m, 12200, &view);
	assert(!strcmp(view.lines[3], "正常供电"));
	for (int64_t t = 12300; t <= 42300; t += 100) {
		monitor_sample(&m, t, true, &sample);
	}
	assert(m.state == MON_OFF);
	sample = (struct measurement){12000, 0, 0};
	monitor_sample(&m, 42400, true, &sample);
	ui_build(&m, 42400, &view);
	assert(!strcmp(view.lines[1], "I: 0.0 mA"));
	assert(!strcmp(view.lines[3], "已断电"));
	assert(!strcmp(view.lines[4], "按键恢复供电"));
	save("off", &view);

	/* 采集失败时立即隐藏旧读数；通信恢复后显示手动恢复提示。 */
	assert(monitor_enable(&m, 42500));
	monitor_sample(&m, 42600, false, NULL);
	ui_build(&m, 42600, &view);
	assert(!strcmp(view.lines[0], "V: -- V"));
	assert(!strcmp(view.lines[1], "I: -- mA"));
	assert(!strcmp(view.lines[2], "P: -- W"));
	assert(!strcmp(view.lines[3], "传感器异常"));
	assert(!strcmp(view.lines[4], "输出已关闭"));
	save("fault", &view);
	monitor_sample(&m, 42700, true, &sample);
	ui_build(&m, 42700, &view);
	assert(!strcmp(view.lines[3], "异常已解除"));
	assert(!strcmp(view.lines[4], "按键恢复供电"));
	assert(!monitor_power_on(&m));
	save("recovered", &view);

	/* 小电流和反向读数的显示精度及符号。 */
	assert(monitor_enable(&m, 42800));
	sample = (struct measurement){12000, -6100, -73200};
	monitor_sample(&m, 42900, true, &sample);
	ui_build(&m, 42900, &view);
	assert(!strcmp(view.lines[1], "I: -6.1 mA"));
	assert(!strcmp(view.lines[2], "P: -0.073 W"));
	button_test();
	puts("ui: V/I/P pages, mA precision, 30s flow, faults and button debounce passed");
}
