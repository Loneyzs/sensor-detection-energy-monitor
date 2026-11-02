#include <zephyr/drivers/pinctrl.h>
#include <zephyr/init.h>

/* This Zephyr version's gpio_mspm0 rejects GPIO_INPUT|GPIO_OUTPUT and its
 * output-only setup clears PINCM.INENA. gpio-i2c falls back to output-only.
 * Reapply GPIO pinctrl with input sensing and open-drain AFTER both buses
 * initialize, keeping their output-enable/data registers untouched.
 */
PINCTRL_DT_DEFINE(DT_NODELABEL(i2c_pad_config));
BUILD_ASSERT(CONFIG_I2C_INIT_PRIORITY < 80, "I2C input fix must run after GPIO I2C init");
static int i2c_inputs_init(void)
{
	return pinctrl_apply_state(PINCTRL_DT_DEV_CONFIG_GET(DT_NODELABEL(i2c_pad_config)),
				   PINCTRL_STATE_DEFAULT);
}
SYS_INIT(i2c_inputs_init, POST_KERNEL, 80);
