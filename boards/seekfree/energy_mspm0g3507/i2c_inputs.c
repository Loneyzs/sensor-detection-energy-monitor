#include <zephyr/drivers/pinctrl.h>
#include <zephyr/init.h>

/* 当前Zephyr的gpio_mspm0拒绝GPIO_INPUT|GPIO_OUTPUT，gpio-i2c回退为纯输出，
 * 初始化会清除PINCM.INENA，导致SDA/SCL无法读回应答和总线电平。
 * 在两条I²C总线初始化完成后，重新应用输入使能、开漏的pinctrl设置，
 * 保留GPIO输出使能和数据寄存器，恢复软件I²C所需的读回能力。
 */
PINCTRL_DT_DEFINE(DT_NODELABEL(i2c_pad_config));
BUILD_ASSERT(CONFIG_I2C_INIT_PRIORITY < 80, "I2C input fix must run after GPIO I2C init");
static int i2c_inputs_init(void)
{
	return pinctrl_apply_state(PINCTRL_DT_DEV_CONFIG_GET(DT_NODELABEL(i2c_pad_config)),
				   PINCTRL_STATE_DEFAULT);
}
SYS_INIT(i2c_inputs_init, POST_KERNEL, 80);
