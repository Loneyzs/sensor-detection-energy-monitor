# 传感与检测技术大作业：ACS712电流监测与待机自动断电

本分支使用 **MSPM0G3507＋ACS712-05B（5A）＋128×64 I²C OLED＋MOSFET模块**，针对12V直流固定电阻负载测量V、I、P、R，低功率持续30秒后断电。OLED、底板PA30按键和PB14开关控制沿用此前已确认正常的硬件。

## 版本保存

- INA219版本保存于标签 **`ina219-stable-20261104`**，对应提交 `ef8bb08`：680Ω／2kΩ、140mW阈值、OLED显示V/I/P/R。
- ACS712版本位于 **`feature/acs712-5a`** 分支。
- INA219原有构建目录 `build-mspm0/`、`build-mspm0-debug/`保留。ACS712默认输出至独立的 `build-acs712/`、`build-acs712-debug/`。

需要取回INA219源码时，可从保存标签建立工作分支：`git switch -c ina219-demo ina219-stable-20261104`。返回本版本使用 `git switch feature/acs712-5a`。

## 两档负载

用户现有3Ω、5Ω、10Ω电阻，均额定50W。选择 **5Ω为高功率正常档、10Ω为低功率待机档**。

| 电阻 | 12V电流 | 12V功耗 | ACS712 OUT标称电压 | 行为 |
|---|---:|---:|---:|---|
| **5Ω／50W** | **2.4A** | **28.8W** | **2.944V** | 保持供电 |
| **10Ω／50W** | **1.2A** | **14.4W** | **2.722V** | 连续低功率30秒后断电 |
| 3Ω／50W | 4A | 48W | 3.240V | 备用；本次演示选用上面两档 |

3Ω在12V下已耗散48W，接近50W额定值；5Ω和10Ω留有更大功率余量。按电阻规格安装散热，带载时电阻会明显发热。电源应提供稳定12V、至少3A，MOSFET模块和负载导线应支持至少3A连续电流；更换负载前先断电。

固件按**实测输入电压×实测电流**判断：**P≥21.6W为正常档，P<21.6W开始待机计时**。两档相差1.2A；12V时阈值对应1.8A。低功率连续30秒后直接关闭PB14，没有额外启动宽限或提醒延时。30秒内功率回升会取消本轮计时。空载和负向功率也归低功率档。

## 接线与板上资源

完整逐线接法、分压图和模块端子说明见 **[MSPM0G3507接线表](docs/MSPM0G3507接线表.md)**。

| 功能 | 资源 | 接法 |
|---|---|---|
| ACS712模拟OUT | **PA25／ADC0_CH2** | **OUT经10kΩ＋10kΩ分压后接P6第4脚**，ADC节点加100nF对地电容 |
| 输入电压 | **PA22／ADC0_CH7** | 底板已连接VCCBAT→10kΩ→PA22→1kΩ→GND，直接使用板载分压 |
| ACS712 VCC／GND | **5V／GND** | P15第7脚／第6脚，ACS712按5V供电 |
| OLED SCL／SDA | **PA1／PA0** | P13第3／4脚；OLED VCC仍接3.3V |
| MOSFET控制 | **PB14** | P13第5脚，高电平导通 |
| 恢复／重新计时 | **PA30** | 底板S2，按下松开一次动作 |
| UART TX／RX | **PB15／PB16** | P7第4／3脚，115200、3.3V电平 |

PA25与CCD、运放模块接口共用，使用本方案时该位置保持无其他模块驱动。PA22的板载分压已占用。ADC1空闲；原INA219的PB2/PB3总线已从本分支移除。两路模拟量由ADC0同一序列采样，OLED继续使用独立GPIO I²C。

ACS712模块的两个螺丝端子IP+、IP−串入负载回路；VCC/GND/OUT是低电流供电和信号接口。OUT经分压后进入3.3V ADC，不能把负载电源接入OUT。ACS712的5V供电位于负载开关之前。

## 采集与显示

ACS712-05B在5V供电下零电流输出约2.5V，标称灵敏度185mV/A。程序上电先保持MOSFET关闭，以8批×32组采样标定实际零点，约0.3秒；零点存于RAM，复位后重新标定，正常运行或故障恢复时沿用已标定值。

电流计算为 `I[µA]=(OUT[µV]-零点[µV])×1000000/185000`。OUT的10kΩ/10kΩ分压乘2还原，PA22的板载10kΩ/1kΩ分压乘11还原输入电压。ADC采用12位、VDDA参考，每次硬件16次平均，再以约1ms间隔采32组做软件平均。ADC单个原始计数约对应8.7mA；50mA以内的电流归零以抑制零点噪声。参数和详细中文注释见 [src/acs712.c](src/acs712.c)、[src/app_config.h](src/app_config.h)。

每次采集完成后再等待100ms。ADC异步请求最多等待100ms，超时期间保持静态缓冲区，待旧请求完成后再重新采样。采集错误、ADC贴近电源轨、输入电压超出8～15V、测得电流超出±5A，或供电期间有效采样间隔超过500ms，均关闭负载。采集恢复后等待PA30手动恢复供电。

OLED同一页显示 **V（V）、I（A）、P（W）、R（Ohm）和两行中文状态**，电流显示到0.01A。R为输入总线电压／负载电流，包含回路中开关与接线的影响；断电、采样无效或电流近零时显示 `--`。显示位数用于排版，实际精度受ACS712灵敏度、零点漂移、5V供电和ADC参考电压影响。

![真实渲染器生成的ACS712页面预览](tests/ui-previews/screens.png)

## 构建和验证

本机工具链：conda `zephyr`环境，Zephyr 4.4.99（`6ab04eeedd2036cdae06538056bc55848f9e5f82`），Zephyr SDK 1.0.1。使用现有MSPM0 ADC驱动，外部Zephyr源码保持完整。

```bash
./scripts/test.sh
python3 tools/preview_oled.py
./scripts/build.sh
ENERGY_BUILD_DIR="$PWD/build-acs712-debug" ./scripts/build.sh -- -DEXTRA_CONF_FILE=debug.conf
```

普通固件输出 `build-acs712/zephyr/zephyr.{elf,hex,bin}`，RTT版输出 `build-acs712-debug/zephyr/`。构建脚本支持 `ZEPHYR_PYTHON`、`ZEPHYR_BASE`、`ZEPHYR_SDK_INSTALL_DIR`、`ENERGY_BUILD_DIR`覆盖。

当前未连接单片机，本次完成源码、主机模拟测试和目标固件构建。零点和两档电流与万用表对照、实际ADC采样稳定性、MOSFET带载和30秒断电，留待接板后验证。详见 [验证记录](docs/验证记录.md)。

## 资料

- [Allegro ACS712数据手册](https://www.allegromicro.com/-/media/Files/Datasheets/ACS712-Datasheet.ashx)：5A型号灵敏度、5V供电、零点与输出负载要求。
- [TI MSPM0G3507数据手册](https://www.ti.com/lit/ds/symlink/mspm0g3507.pdf)：ADC0/ADC1、PA25/PA22通道及模拟输入规格。
- 本仓库底板原理图第4页：R18=10kΩ、R19=1kΩ、C25=100nF与PA22；第5页：PA25接P6第4脚。

`src/`为采集、状态机和显示；`boards/`、`dts/`为板级配置；`tests/`为四组主机测试与预览；`docs/`保留原理图和接线资料。构建产物与本地调试日志不纳入Git。
