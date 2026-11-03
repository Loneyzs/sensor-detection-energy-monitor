#include "ina219.h"
#include <errno.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>

#define INA DT_NODELABEL(ina219)
/* 设备树填分流电阻的实际阻值，单位µΩ；本模块R100=0.1Ω=100000µΩ。 */
#define SHUNT_UOHM DT_PROP(INA, shunt_micro_ohms)
BUILD_ASSERT(SHUNT_UOHM > 0, "Shunt resistance must be positive");
static const struct i2c_dt_spec bus = I2C_DT_SPEC_GET(INA);
static struct ina219_diagnostics diagnostics;

const struct ina219_diagnostics *ina219_diagnostics_get(void)
{
	return &diagnostics;
}

static int failed(enum ina219_step step, int error)
{
	diagnostics.failures[step]++;
	diagnostics.last_failure = step;
	diagnostics.last_error = error;
	return error;
}

static int read_reg(uint8_t reg, uint16_t *value)
{
	uint8_t bytes[2];
	int rc = i2c_write_read_dt(&bus, &reg, 1, bytes, sizeof(bytes));
	if (!rc) {
		/* INA219寄存器为16位，高字节先传输。 */
		*value = sys_get_be16(bytes);
	}
	return rc;
}

int ina219_acquire(struct measurement *sample)
{
	diagnostics.attempts++;
	if (!i2c_is_ready_dt(&bus)) {
		return failed(INA_CONFIG, -ENODEV);
	}
	/* 配置寄存器0x00，组合结果0x3E67，沿用此前实物验证的连续采样配置：
	 * bit13 BRNG=1：母线寄存器采用32V档；芯片输入实际仍须≤26V。
	 * bit12:11 PG=11：分流压降量程±320mV，R100对应约±3.2A。
	 * bit10:7 BADC=1100、bit6:3 SADC=1100：母线/分流各16次平均。
	 * 两路转换合计约17ms；平均用于降低抖动，失调仍由芯片本身决定。
	 * bit2:0 MODE=111：分流和母线连续转换，保持采样期间设备正常响应。
	 * 每轮重新写入配置，再等待转换完成；直接读取分流/母线原始寄存器，
	 * 电流和功率由下方计算，因此无需写校准寄存器或读取电流/功率寄存器。
	 */
	const uint16_t config = (1U << 13) | (3U << 11) | (12U << 7) | (12U << 3) | 7U;
	uint8_t tx[] = {0, config >> 8, config & 0xff};
	int rc = i2c_write_dt(&bus, tx, sizeof(tx));
	if (rc) {
		return failed(INA_CONFIG, rc);
	}
	int64_t deadline = k_uptime_get() + 50;
	uint16_t status = 0;
	/* 母线寄存器0x02的bit1为CNVR；50ms内完成转换才接受本次读数。 */
	do {
		k_msleep(2);
		rc = read_reg(2, &status);
		if (rc) {
			return failed(INA_BUS, rc);
		}
		if (status & 2) {
			break;
		}
	} while (k_uptime_get() < deadline);
	if (!(status & 2)) {
		return failed(INA_CONVERSION, -ETIMEDOUT);
	}
	uint16_t shunt;
	rc = read_reg(1, &shunt);
	if (rc) {
		return failed(INA_SHUNT, rc);
	}
	diagnostics.bus_raw = status;
	diagnostics.shunt_raw = shunt;
	/* 分流寄存器0x01是16位二进制补码，1LSB=10µV。
	 * 先转int16_t恢复正负号，再乘10得到µV；接反时会保留负电流。
	 * 母线寄存器bit15:3为测量值，1LSB=4mV；右移去掉底部状态位。
	 */
	int32_t shunt_uv = (int16_t)shunt * 10;
	int32_t bus_mv = (status >> 3) * 4;
	if (bus_mv > 26000 || shunt_uv >= 320000 || shunt_uv <= -320000) {
		return failed(INA_RANGE, -ERANGE);
	}
	/* 欧姆定律I=Vshunt/Rshunt，按整数单位换算：
	 * I[µA] = Vshunt[µV] × 1000000 / Rshunt[µΩ]。
	 * R100时每个原始计数对应100µA=0.1mA。
	 * 12V、680Ω约17.65mA，对应分流压降约1.765mV、原始值约176；
	 * 12V、2kΩ约6mA，对应0.6mV、原始值约60，两档可稳定分开。
	 * 先转int64_t再乘，防止µV乘1000000时超出32位整数范围。
	 */
	sample->bus_mv = bus_mv;
	sample->current_ua = (int64_t)shunt_uv * 1000000 / SHUNT_UOHM;
	/* P=U×I：mV×µA得到nW，除1000转为µW，供140mW阈值判断。
	 * 使用实测母线电压；例如12000mV×6000µA/1000=72000µW=72mW。
	 */
	sample->power_uw = (int64_t)bus_mv * sample->current_ua / 1000;
	diagnostics.successes++;
	return 0;
}

#ifdef CONFIG_DEBUG_OPTIMIZATIONS
#include <zephyr/sys/printk.h>
volatile uint32_t ina219_debug_scan_request;
void ina219_debug_scan(void)
{
	for (uint16_t address = 0x40; address <= 0x4f; ++address) {
		struct i2c_dt_spec target = bus;
		target.addr = address;
		uint8_t reg = 0, bytes[2] = {0};
		int rc = i2c_write_read_dt(&target, &reg, 1, bytes, 2);
		printk("INA_SCAN addr=0x%02x rc=%d config=%04x\n",
		       address, rc, sys_get_be16(bytes));
	}
}
#endif
