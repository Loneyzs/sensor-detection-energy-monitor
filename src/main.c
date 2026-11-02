#include "ina219.h"
#include "monitor.h"
#include "button.h"
#include "oled.h"
#include "ui_model.h"
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
	printk("12V energy monitor: boot OFF; button=restore/cancel countdown.\n");
	int64_t next_sample = 0, next_report = 0;
	enum monitor_state previous_state = MON_OFF;
	while (true) {
#ifdef CONFIG_DEBUG_OPTIMIZATIONS
		if (ina219_debug_scan_request) {
			ina219_debug_scan_request = 0;
			ina219_debug_scan();
		}
#endif
		int64_t now = k_uptime_get();
		if (now >= next_sample) {
			struct measurement sample;
			rc = ina219_acquire(&sample);
			now = k_uptime_get();
			monitor_sample(&m, now, rc == 0, rc == 0 ? &sample : NULL);
			if (rc && m.state != previous_state) {
				printk("INA219 error: %d; output disabled\n", rc);
			}
			next_sample = now + 100;
		}
		int high = gpio_pin_get_dt(&key);
		if (high < 0) {
			monitor_sample(&m, now, false, NULL);
		} else if (button_update(&button, high != 0, now)) {
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
			uint64_t resistance = 0;
			bool resistance_valid = monitor_resistance_mohm(&m, &resistance);
			printk("DATA valid=%u state=%s bus=%d mV current=%d uA power=%lld uW "
			       "energy=%llu uWh R_valid=%u R=%llu mOhm oled=%u\n",
			       m.valid, monitor_state_name(m.state), m.reading.bus_mv,
			       m.reading.current_ua, (long long)m.reading.power_uw,
			       (unsigned long long)(m.energy_uw_ms / 3600000), resistance_valid,
			       (unsigned long long)resistance, oled_is_ready());
			const struct ina219_diagnostics *d = ina219_diagnostics_get();
			printk("INA_DIAG n=%u ok=%u fail_cfg=%u fail_bus=%u fail_shunt=%u "
			       "timeout=%u range=%u last=%d step=%u raw=%04x/%04x\n",
			       d->attempts, d->successes, d->failures[INA_CONFIG],
			       d->failures[INA_BUS], d->failures[INA_SHUNT],
			       d->failures[INA_CONVERSION], d->failures[INA_RANGE],
			       d->last_error, d->last_failure, d->bus_raw, d->shunt_raw);
			next_report = now + 1000;
		}
		k_msleep(10);
	}
	return 0;
}
