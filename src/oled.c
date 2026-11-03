#include "oled.h"
#include "ssd1306.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>

static struct monitor snapshot;
static uint8_t frame[OLED_FRAME_BYTES];
static atomic_t ready;
#ifdef CONFIG_DEBUG_OPTIMIZATIONS
/* J-Link调试时可暂停OLED传输，对照检查采样通信。 */
volatile uint32_t oled_debug_pause; /* 1=暂停传输，2=关屏并暂停传输 */
#endif
/* 主线程发布测量快照，显示线程加锁复制后再发I²C，避免阻塞负载控制。 */
K_MUTEX_DEFINE(snapshot_lock);

void oled_publish(const struct monitor *m)
{
	k_mutex_lock(&snapshot_lock, K_FOREVER);
	snapshot = *m;
	k_mutex_unlock(&snapshot_lock);
}

bool oled_is_ready(void)
{
	return atomic_get(&ready) != 0;
}

static void display_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
	bool initialized = false;
#ifdef CONFIG_DEBUG_OPTIMIZATIONS
	bool debug_blanked = false;
#endif
	bool reported_failure = false;
	while (true) {
#ifdef CONFIG_DEBUG_OPTIMIZATIONS
		if (oled_debug_pause) {
			if (oled_debug_pause == 2 && !debug_blanked) {
				int rc = ssd1306_set_display(false);
				printk("OLED_DIAG display_off rc=%d\n", rc);
				debug_blanked = rc == 0;
			}
			k_msleep(50);
			continue;
		}
		if (debug_blanked) {
			(void)ssd1306_set_display(true);
			debug_blanked = false;
		}
#endif
		int rc = 0;
		if (!initialized) {
			rc = ssd1306_init();
			initialized = rc == 0;
		}
		if (!rc) {
			struct monitor m;
			struct ui_view view;
			k_mutex_lock(&snapshot_lock, K_FOREVER);
			m = snapshot;
			k_mutex_unlock(&snapshot_lock);
			ui_build(&m, k_uptime_get(), &view);
			ui_render(&view, frame);
			rc = ssd1306_update(frame);
		}
		if (rc) {
			atomic_clear(&ready);
			initialized = false;
			if (!reported_failure) {
				printk("OLED unavailable: %d; retry in 2s\n", rc);
				reported_failure = true;
			}
			k_msleep(2000);
		} else {
			if (!oled_is_ready()) {
				printk("OLED ready (SSD1306 128x64)\n");
			}
			atomic_set(&ready, 1);
			reported_failure = false;
			k_msleep(200);
		}
	}
}

K_THREAD_DEFINE(oled_thread, 3072, display_thread, NULL, NULL, NULL, 7, 0, SYS_FOREVER_MS);

void oled_start(const struct monitor *m)
{
	oled_publish(m);
	k_thread_start(oled_thread);
}
