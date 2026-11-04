# 传感与检测技术课程大作业：直流功率监测与待机自动断电

**题目：基于 MSPM0G3507 和 INA219 的低压直流设备功率监测与待机自动断电系统设计与制作。**

使用 MSPM0G3507 核心板与底板、INA219、128×64 I²C OLED 和高电平导通的 MOSFET 模块，基于 Zephyr 树外开发。演示负载固定为 **12V下的5Ω／50W和10Ω／50W电阻**，OLED仅显示V、I、P、R和中文状态，底板PA30用于恢复供电或重新计时。

## 两档负载与判断逻辑

| 演示负载 | 12V理论电流 | 理论功耗 | 行为 |
|---|---|---|---|
| **5Ω／50W** | **2.353A** | **27.682W** | 正常供电 |
| **10Ω／50W** | **1.188A** | **14.116W** | 连续低功率30秒后直接断电 |

上表按12V输入、0.1Ω串联分流电阻、忽略开关和接线损耗计算，`I=12/(R+0.1)`、`P负载=I²R`。对应分流压降约235.3mV／118.8mV，处于当前±320mV量程内。

固件按INA219的**实测功率**判断：**P≥21W保持正常供电，P<21W进入待机计时**，空载和反向功率也按低功率处理。两档电流相差约1.165A，R100分流电阻下的换算分辨率为0.1mA。功率在30秒内恢复到21W及以上时取消本轮计时；再次降低后从零开始。删除原有2W超限分支、3秒启动宽限和5秒提醒；从首次低功率有效采样起，到达30秒的首次有效采样关闭MOSFET，实际动作受约0.13秒采样周期影响。

两只负载均为50W功率电阻，使用鳄鱼夹连接，按电阻规格安排散热并在断电后更换。用户确认电源、MOSFET及连接线满足带载要求。新模块采用0.1Ω、2512封装分流电阻，用户估计额定1W或2W；5Ω档分流耗散约0.55W，按至少1W规格使用。3Ω在本电路中约3.87A、分流387mV，超出当前量程，本版本使用5Ω／10Ω。

原小电流INA219版本保存在标签 `ina219-stable-20261104`；ACS712方案保留于 `feature/acs712-5a` 分支。当前 `main` 为INA219＋5Ω／10Ω方案。

## 按键、恢复与异常断电

上电或复位后 **PB14=低，负载关闭**。INA219采集正常后，按下并松开 **底板S2／PA30** 接通负载；建议按住约0.2秒后松开。按键消抖30ms，一次完整按下、松开产生一次动作。

- 待机计时期间按PA30，下一次有效采样重新开始判断和计时。
- 自动断电后保持关闭，按PA30恢复；恢复后重新按实测功率判断。
- INA219通信失败、转换轮询超过50ms、母线电压超过26V、分流压降达到±320mV，或供电期间有效采样间隔超过500ms，均关闭负载并保持故障状态。
- 采集恢复后显示“异常已解除／按键恢复供电”，等待PA30恢复。

## 软件完成情况与硬件待验证项

软件已按两档负载完成实现，主机测试与普通/RTT固件构建结果见 [验证记录](docs/验证记录.md)。中文注释覆盖状态流转、按键、OLED刷新及INA219寄存器、电流/功率单位换算。

用户已确认OLED、按键和MOSFET等硬件此前正常；旧INA219因意外短路烧毁，已更换新模块。后续重点验证**新INA219的通信、零点及5Ω/10Ω读数**，随后确认两档判定与30秒断电联动。软件侧工作已完成，新模块的测量与整机联调验收待实物调试。

## 接线

详细逐线连接、电源位置和负载回路见 [MSPM0G3507接线表](docs/MSPM0G3507接线表.md)。GPIO名称统一使用PA/PB形式。

| 功能 | GPIO | 底板位置 |
|---|---|---|
| OLED SCL / SDA | **PA1 / PA0** | ToF接口P13第3 / 第4脚 |
| INA219 SCL / SDA | **PB2 / PB3** | 无线串口接口P15第4 / **第8脚**，第6脚为GND |
| MOSFET控制 | **PB14** | P13第5脚，高电平导通，低电平断电 |
| 恢复／重新计时 | **PA30** | 底板S2，按下为低 |
| UART TX / RX | PB15 / PB16 | P7第4 / 第3脚，115200、8N1、3.3V电平 |
| SWDIO / SWCLK | PA19 / PA20 | 核心板SWD接口 |

OLED和INA219分用两组独立总线，通过Zephyr `gpio-i2c` 开漏驱动，配置100kHz。当前Zephyr版本尚无对应的MSPM0硬件I²C驱动；本仓库包含GPIO输入读回适配。OLED自动尝试0x3C、0x3D；INA219默认0x40，分流电阻R100（0.1Ω）。[新模块原理图](docs/INA219原理图.png)已纳入仓库，详细模块引脚见接线表。

由同一12V电源分支供给底板XT入口和负载，控制逻辑位于负载开关之前。OLED／INA219的 **VCC接3.3V**，各模块共地。MOSFET控制端兼容3.3V，并需要硬件下拉保证复位期间关闭。PA14固定低电平关闭底板蜂鸣器，因此核心板LED2可常亮；该灯不表示负载状态。

本次使用的J-Link“3.3V”线具备供电能力，**外部电源开启时断开这根线，只保留GND、PA19、PA20**。本机调试器已设置固定VTref=3.3V并验证三线调试可用。标准调试器的真正VTref检测输入按其手册连接；核心板SWD电源脚的资料／实物差异见接线表。

## OLED与测量

同一页仅显示 **V（V）、I（mA）、P（W）、R（Ohm）和中文状态提示**。电压保留0.01V、电流保留0.1mA、功率保留0.001W，等效阻值R按实测电压/电流计算并保留0.1Ω；累计电量E已删除。断电、采样无效、电压非正或电流低于1mA（含反向）时，R显示 `--`。待机期间继续显示实时读数，并显示“待机计时 n/30秒”；断电后显示“已断电／按键恢复供电”。采集失败时四项数值均显示 `--`，恢复后显示“异常已解除／按键恢复供电”。

![真实渲染器生成的页面预览](tests/ui-previews/screens.png)

INA219沿用连续转换配置0x3E67，母线和分流ADC各16次平均。分流寄存器为有符号数，每计数10µV；R100=0.1Ω时每计数对应100µA=0.1mA。电流按 `I[µA]=Vshunt[µV]×1000000/Rshunt[µΩ]` 计算，功率按 `P[µW]=U[mV]×I[µA]/1000` 计算。使用64位中间值避免乘法溢出，原始读数、转换状态和失败计数保留在调试日志中。详细中文注释见 [src/ina219.c](src/ina219.c)。

每次转换完成后再等待100ms；待机计时使用实际系统时间。OLED在独立线程刷新，快照用互斥锁保护；显示通信失败后每2秒重试。INA219使用另一条总线。低边开关断开后，INA219对地母线电压可继续显示约12V，电流和功率应回落至零点附近。

## 构建、烧录和测试

本机使用conda `zephyr`环境、Zephyr 4.4.99（提交 `6ab04eeedd2036cdae06538056bc55848f9e5f82`）、Zephyr SDK 1.0.1。板级定义、设备树与应用均在本仓库内，外部Zephyr源码无需修改。默认目标 `energy_mspm0g3507`，80MHz、Flash128KB、RAM32KB。

已有Zephyr工作区时：

```bash
./scripts/test.sh
./scripts/build.sh
```

构建脚本默认使用 `~/anaconda3/envs/zephyr/bin/python`、`~/zephyrws/zephyr`、`~/zephyr_sdk`，可通过 `ZEPHYR_PYTHON`、`ZEPHYR_BASE`、`ZEPHYR_SDK_INSTALL_DIR` 覆盖。普通版本输出 `build-mspm0/zephyr/zephyr.{elf,hex,bin}`，日志走UART2。

```bash
conda activate zephyr
export ZEPHYR_BASE="$HOME/zephyrws/zephyr"
export ZEPHYR_SDK_INSTALL_DIR="$HOME/zephyr_sdk"
west flash -d build-mspm0 --runner jlink

# J-Link RTT调试版本
ENERGY_BUILD_DIR="$PWD/build-mspm0-debug" ./scripts/build.sh -- -DEXTRA_CONF_FILE=debug.conf
west flash -d build-mspm0-debug --runner jlink
```

全新环境可在安装Zephyr SDK、Python及west后，通过本仓库 `west.yml` 获取固定版本依赖：

```bash
west init -m https://github.com/Loneyzs/sensor-detection-energy-monitor --mr main energy-workspace
cd energy-workspace
west update
west zephyr-export
pip install -r zephyr/scripts/requirements.txt
cd energy-monitor
west build -b energy_mspm0g3507 -d build-mspm0 .
```

参数集中在 [src/app_config.h](src/app_config.h)：低功率阈值21000mW（21W）、待机30秒、有效采样最大间隔500ms，修改后重新构建烧录。

主机测试启用AddressSanitizer和UndefinedBehaviorSanitizer，覆盖两档负载、21W阈值边界、连续30秒断电、切回正常档取消计时、按键重新计时和恢复、采集故障锁存、采样间隔异常、OLED总线传输与INA219原始值换算。测试同时生成PBM页面，`python tools/preview_oled.py` 可更新PNG总览，需要Pillow。

## 目录与验证

- `src/`：监测状态机、采集、按键、OLED驱动与显示。
- `boards/`、`dts/`：MSPM0G3507板级定义、两路I²C与GPIO绑定。
- `tests/`：四组主机测试及OLED页面预览。
- `docs/`：核心板／底板原理图、传感器／OLED资料、接线表和验证记录。
- `tools/`：字体子集与OLED预览生成脚本；字体许可保留在docs中。

[验证记录](docs/验证记录.md)区分实物验收与主机模拟测试。构建目录和本地调试备份不纳入Git仓库。硬件资料与字体保留原作者的权利及许可说明。
