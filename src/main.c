#include "acs712.h"
#include "monitor.h"
#include "button.h"
#include "oled.h"
#include <errno.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define CTRL DT_NODELABEL(controls)
static const struct gpio_dt_spec power = GPIO_DT_SPEC_GET(CTRL, power_gpios);
static const struct gpio_dt_spec key = GPIO_DT_SPEC_GET(CTRL, confirm_gpios);

static int io_init(void)
{
	if (!gpio_is_ready_dt(&power)) {
		return -ENODEV;
	}
	/* PB14高电平导通，上电先配置为低电平，等待用户按键接通负载。 */
	int rc = gpio_pin_configure_dt(&power, GPIO_OUTPUT_INACTIVE);
	if (rc) {
		return rc;
	}
	if (!gpio_is_ready_dt(&key)) {
		return -ENODEV;
	}
	return gpio_pin_configure_dt(&key, GPIO_INPUT);
}

static int outputs_update(const struct monitor *m)
{
	return gpio_pin_set_dt(&power, monitor_power_on(m));
}

int main(void)
{
	struct monitor m;
	struct button button = {0};
	monitor_init(&m);
	int rc = io_init();
	if (rc) {
		printk("GPIO init failed: %d\n", rc);
		return rc;
	}
	oled_start(&m);
	printk("12V ACS712-05B monitor: boot OFF; button=restore/cancel countdown.\n");
	int64_t next_sample = 0, next_report = 0;
	enum monitor_state previous_state = MON_OFF;
	while (true) {
		int64_t now = k_uptime_get();
		if (now >= next_sample) {
			struct measurement sample;
			rc = acs712_acquire(&sample);
			now = k_uptime_get();
			monitor_sample(&m, now, rc == 0, rc == 0 ? &sample : NULL);
			if (rc && m.state != previous_state) {
				printk("ACS712 error: %d; output disabled\n", rc);
			}
			/* 转换完成后等待100ms；状态机使用实际时间计算30秒时长。 */
			next_sample = now + 100;
		}
		/* PA30配置为低有效，gpio_pin_get_dt返回1表示物理按下。 */
		int pressed = gpio_pin_get_dt(&key);
		if (pressed < 0) {
			monitor_sample(&m, now, false, NULL);
		} else if (button_update(&button, pressed != 0, now)) {
			if (!monitor_enable(&m, now)) {
				printk("Enable refused: measurement unavailable\n");
			} else {
				printk("Button: supply enabled/countdown restarted\n");
			}
		}
		rc = outputs_update(&m);
		if (rc) {
			(void)gpio_pin_set_dt(&power, 0);
			monitor_sample(&m, now, false, NULL);
			oled_publish(&m);
			printk("GPIO output failed: %d\n", rc);
			return rc;
		}
		oled_publish(&m);
		if (m.state != previous_state) {
			printk("STATE %s -> %s\n", monitor_state_name(previous_state),
			       monitor_state_name(m.state));
			previous_state = m.state;
		}
		if (now >= next_report) {
			printk("DATA valid=%u state=%s bus=%d mV current=%d uA power=%lld uW "
			       "oled=%u\n",
			       m.valid, monitor_state_name(m.state), m.reading.bus_mv,
			       m.reading.current_ua, (long long)m.reading.power_uw,
			       oled_is_ready());
			const struct acs712_diagnostics *d = acs712_diagnostics_get();
			printk("ACS_DIAG n=%u ok=%u fail=%u last=%d calibrated=%u "
			       "raw=%u/%u out=%d uV zero=%d uV\n",
			       d->attempts, d->successes, d->failures, d->last_error,
			       d->calibrated, d->current_raw, d->bus_raw,
			       d->output_uv, d->zero_uv);
			next_report = now + 1000;
		}
		k_msleep(10);
	}
	return 0;
}
