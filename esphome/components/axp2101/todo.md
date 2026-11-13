# AXP2101 实现状态与待办

> 数据手册：X-Powers **AXP2101 Datasheet V1.0**（下称“手册”）。
> 寄存器数值全部逐条对照手册核对；手册自相矛盾处以显式码表为准，差异记录在
> `axp2101.cpp` 的注释和本文件第 5 节。
> ESPHome 稳定版基线 2026.10.2，本仓库运行在 2026.11.0-dev。

## 1. 架构

一张共享机制层加两张表：

| 层 | 位置 | 职责 |
| --- | --- | --- |
| 共享 hub | `components/axp_base/` | 寄存器读写、电源轨控制、电压编解码、ADC、状态缓存、中断、一次性控制、select/number 数值表机制。它不知道自己在跟哪颗芯片说话。 |
| 型号表 | `components/axp2101/axp2101.cpp` | 纯数据：芯片 ID、14 路电源轨、5 个 ADC 通道、7 个状态位、24 个中断源、4 张选项表、5 个线性设定、2 个开关位（充电/电量计）、3 个一次性控制位（关机/重启/电量计复位）。 |
| 实体 | `components/axp2101/<平台>/` | 每个平台一层薄壳，把 YAML 配置落到 hub 方法上。实体类放在芯片自己的平台目录里。 |

约定（均为已经落地的设计决定）：

- `axp_base` 不 `AUTO_LOAD` 任何平台；只有配置里真的出现 `platform: axp2101`
  才会引入对应平台，只配 hub 时 `esphome.h` 里没有任何平台头文件。
- 一条电源轨只能被 `switch` / `output` 其中之一驱动，第二个认领者会在最终校验
  阶段报错（不是等到生成代码才报）。
- 不做“禁止关闭”保护：这颗芯片的用途就是彻底切断 MCU，实体由用户自己决定是否
  `internal:`。
- 数值只维护一份：量纲、寄存器位、档位表都在 C++ 型号表；Python 侧只保留名称、
  单位、上下界，并各有一个测试把两边钉死（改错会挂单测，而不是悄悄选到隔壁档）。

## 2. 已完成功能

### 2.1 芯片与总线

| 功能 | 手册 | 实现 | 状态 |
| --- | --- | --- | --- |
| 芯片 ID 校验（`REG 03H` = `0x4A`） | 6.13 寄存器表 | `AxpComponent::setup()` | ✅ |
| I2C 地址（默认 `0x34`） | — | `axp2101` 配置模式 | ✅ |
| 状态寄存器缓存轮询（`update_interval`，默认 60s） | `REG 00H/01H` | `update_status()` + `status_bit()` | ✅ |
| IRQ 引脚（低电平有效、下降沿中断） | 引脚 38 | `set_irq_pin()` | ✅ |
| 无 IRQ 引脚时轮询中断状态寄存器 | `REG 48H-4AH` | `event_poll_interval`（默认 100ms，可 `never`） | ✅ |
| 中断状态写 1 清零、批量读出 | `REG 48H-4AH` | `read_and_clear_events()` | ✅ |
| 中断使能（只开被订阅的源） | `REG 40H-42H` | `add_events_enabled()`（累加，不互相踩） | ✅ |

### 2.2 电源轨（14 路）

`switch` 负责 0/1 开关，`output` 负责连续调压（0 电平=关闭）。带 DVM 的转换器
可以在设定点之间斜坡过渡。

| 电源轨 | 电压范围 | 步进 | 最大电流 | 开关 | 调压 | DVM |
| --- | --- | --- | --- | --- | --- | --- |
| DCDC1 | 1.5–3.4 V | 100 mV | 2000 mA | ✅ | ✅ | — |
| DCDC2 | 0.5–1.2 V / 1.22–1.54 V | 10 / 20 mV | 2000 mA | ✅ | ✅ | ✅ |
| DCDC3 | 0.5–1.2 / 1.22–1.54 / 1.6–3.4 V | 10 / 20 / 100 mV | 2000 mA | ✅ | ✅ | ✅ |
| DCDC4 | 0.5–1.2 / 1.22–1.84 V | 10 / 20 mV | 1500 mA | ✅ | ✅ | — |
| DCDC5 | 1.4–3.7 V | 100 mV | 1000 mA | ✅ | ✅ | — |
| ALDO1–4 | 0.5–3.5 V | 100 mV | 300 mA | ✅ | ✅ | — |
| BLDO1–2 | 0.5–3.5 V | 100 mV | 300 mA | ✅ | ✅ | — |
| CPUSLDO | 0.5–1.4 V | 50 mV | 30 mA | ✅ | ✅ | — |
| DLDO1 | 0.5–3.3 V | 100 mV | 300 mA | ✅ | ✅ | — |
| DLDO2 | 0.5–1.4 V | 50 mV | 300 mA | ✅ | ✅ | — |

配套能力：

| 功能 | 实现 | 状态 |
| --- | --- | --- |
| 精确设定（必须落在步进网格上） | `set_rail_voltage()` | ✅ |
| 就近取档（调光用，回报实际写入值） | `set_rail_voltage_nearest()` | ✅ |
| 区间内可达档位数量统计 | `count_rail_levels()` | ✅ |
| DVM（仅 DCDC2/DCDC3） | `set_rail_dvm()` | ✅ |
| 与 `light` 的 `monochromatic` 平台联动（电压线性映射） | `output` + `platform: monochromatic` | ✅ |

### 2.3 采集（`sensor`，6 个）

| 实体 | 手册 | 单位 | 状态 |
| --- | --- | --- | --- |
| `battery_voltage` | `REG 34H/35H` | V | ✅ |
| `battery_level`（电量计 SOC） | `REG A4H` | % | ✅ |
| `vbus_voltage` | `REG 38H/39H` | V | ✅ |
| `system_voltage`（VSYS） | `REG 3AH/3BH` | V | ✅ |
| `ts_voltage`（TS 引脚电压） | `REG 36H/37H` | V | ✅ |
| `die_temperature`（芯片温度） | `REG 3CH/3DH` | °C | ✅ |

`REG 30H` 里除了电池电压和 TS 两个通道，其余上电默认是关闭的，所以每个 ADC 实体
在 `setup()` 里打开自己要用的那一位；14 位拼接读取，总线失败时不发布任何值。

### 2.4 状态（`binary_sensor`，7 个）

| 实体 | 手册 | 状态 |
| --- | --- | --- |
| `battery_present` | `REG 00H` b3 | ✅ |
| `battery_charging` | `REG 01H` b6:5 = 01 | ✅ |
| `battery_discharging` | `REG 01H` b6:5 = 10 | ✅ |
| `vbus_good` | `REG 00H` b5 | ✅ |
| `battery_active_mode` | `REG 00H` b2 | ✅ |
| `thermal_regulation` | `REG 00H` b1 | ✅ |
| `current_limit` | `REG 00H` b0 | ✅ |

### 2.5 中断事件（`event`，24 个）

`event` 平台默认收录全部 24 个中断源，也可以用 `events:` 只订阅其中几个（多个
实体各自订阅不同源时，使能位取并集，互不覆盖）。事件名即选项名，直接对应
`REG 48H-4AH` 的 24 个状态位：

`REG 48H`：`soc_warning_level`、`soc_shutdown_level`、`gauge_watchdog_timeout`、
`gauge_new_soc`、`battery_over/under_temperature_charging`、
`battery_over/under_temperature_working`

`REG 49H`：`vbus_insert`、`vbus_remove`、`battery_insert`、`battery_remove`、
`power_key_short_press`、`power_key_long_press`、`power_key_negative_edge`、
`power_key_positive_edge`

`REG 4AH`：`watchdog_expired`、`ldo_over_current`、`batfet_over_current`、
`charge_done`、`charge_started`、`die_over_temperature`、
`charger_safety_timer`、`battery_over_voltage`

状态：✅

### 2.6 一次性控制（`button` + 同名 action，3 个）

| 实体 / action | 手册 | 寄存器 | 状态 |
| --- | --- | --- | --- |
| `shutdown` / `axp2101.shutdown` | 6.13.2.7 | `REG 10H` b0（软关机） | ✅ |
| `restart` / `axp2101.restart` | 6.13.2.7 | `REG 10H` b1（复位 SoC 并重跑上电时序） | ✅ |
| `reset_gauge` / `axp2101.reset_gauge` | 6.13.2.13 | `REG 17H` b3（电量计重新校准） | ✅ |

读改写实现，不破坏同寄存器里的配置位；芯片没有该项时打 warning，而不是静默无效。

### 2.7 选项（`select`，4 个）

| 设定 | 手册 | 选项 | 状态 |
| --- | --- | --- | --- |
| `charge_current` | 6.13.2.60 `REG 62H` b4:0 | 100–1000 mA，13 档（0 mA 故意不列） | ✅ |
| `charge_voltage` | 6.13.2.62 `REG 64H` b2:0 | 4.0 / 4.1 / 4.2 / 4.35 / 4.4 V | ✅ |
| `input_current_limit` | 6.13.2.12 `REG 16H` b2:0 | 100 / 500 / 900 / 1000 / 1500 / 2000 mA | ✅ |
| `power_key_long_press` | 6.13.2.24 `REG 27H` b5:4 | 1 / 1.5 / 2 / 2.5 s | ✅ |

### 2.8 线性设定（`number`，5 个）

| 设定 | 手册 | 范围 / 步进 | 单位 | 状态 |
| --- | --- | --- | --- | --- |
| `battery_low_warning` | 6.13.2.16 `REG 1AH` b7:4 | 5–20 / 1 | % | ✅ |
| `battery_low_shutdown` | 6.13.2.16 `REG 1AH` b3:0 | 0–15 / 1 | % | ✅ |
| `input_voltage_limit`（VINDPM） | 6.13.2.11 `REG 15H` b3:0 | 3880–5080 / 80 | mV | ✅ |
| `precharge_current` | 6.13.2.59 `REG 61H` b3:0 | 0–200 / 25 | mA | ✅ |
| `termination_current` | 6.13.2.61 `REG 63H` b3:0 | 0–200 / 25 | mA | ✅ |

写入时就近取档并回读实际值；手册保留的编码（如 `REG 61H` 的 9–15）不接受也
不发布。

### 2.9 芯片开关（`switch` 子键，2 个）

与电源轨开关共用 `switch` 平台，但用子键形式配置，一条配置项里可以放多个：

| 设定 | 手册 | 说明 | 状态 |
| --- | --- | --- | --- |
| `charging` | 6.13.2.14 `REG 18H` b1 | 电芯充电总开关，关掉即停止给电池充电 | ✅ |
| `gauge` | 6.13.2.14 `REG 18H` b3 | 电量计模块开关；关掉后 `battery_level` 会读 0 | ✅ |

```yaml
switch:
  - platform: axp2101
    rail: ALDO3          # 电源轨开关：一条配置项一个
    name: Peripherals
  - platform: axp2101
    charging:            # 芯片设定开关：一条配置项可以放多个
      name: Charging
    gauge:
      name: Fuel Gauge
      restore_mode: RESTORE_DEFAULT_ON
```

行为约定：

- 默认 `restore_mode: DISABLED`：上电时**只读取并上报**芯片自己的状态，不写寄存器。
  这样加一个 `charging:` 实体不会悄悄把充电关掉；要固定状态时显式写 `restore_mode`
  或 `ALWAYS_ON` / `ALWAYS_OFF`。
- 与电源轨相同，同一个设定只能被一个实体驱动（重复出现会在最终校验阶段报错）。
- 对应的 hub API 仍然可用：`set_charging_enabled()` / `is_charging_enabled()` /
  `set_gauge_enabled()` / `is_gauge_enabled()`。

## 3. 未实现功能

按“手册里的功能”逐项列出，未打勾的都还没做。

### 3.1 完全未建模

| 功能 | 手册 | 说明 | 建议优先级 |
| --- | --- | --- | --- |
| 充电截止电流使能 | 6.13.2.61 `REG 63H` b4 | 0/1，决定是否用 `termination_current` 判满 | 中（和已有的 `termination_current` 配套） |
| 充电安全定时器 | 6.13.2.64 `REG 67H` | 预充 40/50/60/70 min、恒流 5/8/12/20 h、DPM 期间是否放慢 | 中（4 档枚举，适合 `select`） |
| 电池检测使能 | 6.13.2.65 `REG 68H` b0 | 插入检测总开关 | 低 |
| CHGLED 行为 | 6.13.2.66 `REG 69H` | 充电指示灯的控制来源与闪烁方式 | 低 |
| 纽扣电池充电截止电压 | 6.13.2.67 `REG 6AH` | 备用电池充电目标 | 低 |
| 看门狗 | 6.13.2.14/15 `REG 18H` b0 + `REG 19H` | 超时可 IRQ / 复位 SoC / 拉低 PWROK 等；喂狗必须在主循环里 | **中高**，应做成 hub 配置项（如 `watchdog: 8s`），不是实体 |
| 芯片过温保护等级 / 检测使能 | 6.13.2.8/9 `REG 12H`、`REG 13H` | 115/125/135 °C | 中（`select`） |
| 最小系统电压（Vsys DPM） | 6.13.2.10 `REG 14H` b6:4 | 4.1–4.8 V，8 档 | 中（`select`） |
| 热调节阈值 | 6.13.2.63 `REG 65H` b1:0 | 60/80/100/120 °C | 中（`select`） |
| PWRON 长按关机时间 / 开机时间 | 6.13.2.24 `REG 27H` b3:2（OFFLEVEL，4/6/8/10 s）、b1:0（ONLEVEL，128/512 ms、1/2 s） | 目前只做了 b5:4（IRQLEVEL） | 低-中 |
| 关机行为配置 | 6.13.2.19–22 `REG 22H`–`REG 25H` | 按键关机是关机还是重启、DCDC OVP/UVP 关机、Vsys 关机阈值、PWROK 时序 | 低（风险高，配错会变砖或无法唤醒） |
| 睡眠 / 唤醒控制 | 6.13.2.23 `REG 26H` | IRQ 低电平唤醒、唤醒时电压选择 | 低（与 ESPHome `deep_sleep` 职责重叠） |
| 快速开机时序 | 6.13.2.25–28 `REG 28H`–`REG 2BH` | 每路电源的上电顺序 | 不建议暴露（属于硬件设计参数） |
| TS 引脚控制 / TS 窗口 / JEITA | 6.13.2.46–56 `REG 50H`–`REG 5BH` | JEITA 会改写充电电流/电压；目前只读 `ts_voltage` | 中（作为充电配置项，而非实体） |
| DCDC 强制 PWM（CCM） | 6.13.2.68 `REG 80H` b6 | 全转换器强制 CCM，影响轻载纹波与效率 | 中（可作为 `output` 的可选项） |
| 单路 PWM/PFM、扩频、UVP 消抖 | 6.13.2.69 `REG 81H` b5:2、b7:6、b1:0 | 每路 DCDC 是否常开 PWM、开关频率扩频范围、欠压消抖时间 | 低 |
| DVM 斜坡速率 | 6.13.2.68 `REG 80H` b5 | 15.625 / 31.25 µs 每档；目前只开关 DVM，不改斜率 | 低-中（可作为 `output` 的 `dvm` 子选项） |
| 电池参数 / 电量计控制 | `REG A1H`、`REG A2H` | 电量计内部参数 | 低 |
| DC1SW / DC4SW 复用输出 | 引脚 11、20 | `DLDO1/DC1SW`、`DLDO2/DC4SW` 复用；软件只能在 LDO 与开关之间切 | 低（硬件设计决定） |
| 数据缓冲 `REG 04H`–`07H` | 6.13.2.3–6 | 通用 4 字节暂存 | 不做（无用途） |

### 3.2 已明确不做

| 功能 | 原因 |
| --- | --- |
| 用 `number` 直接给电源轨调压 | 会与 `output` 争夺同一条轨；`output` + `light`（或模板 number 换算电平）已覆盖该需求 |
| GPIO / IO expander | **AXP2101 没有可软件控制的 GPIO**：寄存器表 0x00–0xA4 内没有 GPIO 控制块，引脚 32 的 `GPIO1/FB5/RTC` 由芯片定制（EFUSE）决定是 RTCLDO2 输出还是 DCDC5 反馈。GPIO 扩展留给 AXP192（它有 GPIO0–GPIO4）。 |
| 充电电流/电压再做成 `number` | 已有 `select`，两套实体会写同一个寄存器 |
| 电源轨“禁止关闭”保护 | 用户明确不需要（芯片用途就是彻底断电） |

## 4. 测试与验证

| 项目 | 命令 | 结果 |
| --- | --- | --- |
| hub 单测 | `script/cpp_unit_test.py axp_base` | 69 项通过 |
| 芯片单测（含各平台子目录） | `script/cpp_unit_test.py axp2101` | 81 项通过 |
| Python 单测 | `pytest tests/unit_tests/components/test_axp2101_*.py` | 72 项通过（rails 17 / event 12 / button 15 / select 9 / number 10 / switch 9） |
| YAML 编译 | `script/test_build_components -e compile -c axp2101 -t esp32-idf` | 通过（`tests/components/axp2101/common.yaml` 覆盖全部平台） |
| 代码风格 | `clang-format`、`ruff`、`pylint`、`script/ci-custom.py "axp_base\|axp2101"` | 全部通过 |

单测里值得注意的几条：

- 假 I2C 总线按真机行为建模了中断状态寄存器的**写 1 清零**；否则排空中断的那 4 轮
  循环会一直以为还有事件没取走。
- 每条电源轨的电压码表都做了“编码→解码→再编码”全码域往返测试。
- `select` / `number` 的选项名与上下界会从 C++ 型号表反推，和 Python 侧逐项比对。

## 5. 手册已知矛盾（实现时的取舍）

| 位置 | 手册写法 | 采用值 | 原因 |
| --- | --- | --- | --- |
| `REG 90H` b4 | “aldo1 enable”（与 b0 重复） | BLDO1 | 按 ALDO1-4/BLDO1-2/CPUSLDO/DLDO1 填 bits 0-7、BLDO2 单独在 `REG 91H` 的顺序，b4 只能是 BLDO1 |
| `REG 98H`/`REG 9AH` 电压窗 | 码表写 code 19 = 1.40 V，但 0.5 V + 50 mV × 19 = 1.45 V | code 18（1.40 V）为顶 | 只取两种读法都同意的最高档，避免高出 50 mV |
| `REG 99H`（DLDO1） | 标题写 0.5–3.4 V，码表止于 `11100` = 3.3 V | 3.3 V | 以显式码表为准 |
| `REG 16H` 档位表 | 1.5 A / 2 A | 1500 mA / 2000 mA | 全组件统一用 mA，便于和 `charge_current` 对齐 |

## 6. 后续

1. **AXP192**：仿照本芯片补型号表（电源轨、ADC 含 GPIO0–3 通道、状态位、中断事件
   ——它有 4 个 IRQ 状态寄存器 `REG 44H`–`47H`、选项/线性表），再复用同一套平台。
2. **AXP192 GPIO 扩展**：GPIO0–GPIO4 有真正的控制寄存器（`REG 90H`–`97H`）：
   GPIO0–2 可作 LDO 输出 / NMOS 开漏输出 / 输入（GPIO0 还有独立电压寄存器 `REG 91H`），
   GPIO0–3 可作 ADC 输入，GPIO3/4 可作开漏输出或输入（`REG 95H`/`96H`）。
3. **旧 `components/axp192`**（`axp192.cpp` 2279 行，由旧 AXP2101 代码查找替换而来，
   留有 30 余处 `//was 0xXX` 补丁标记和若干功能性错误，例如 `enableALDO1()` 写的是
   `REG 12H` b0，在 AXP192 上这是 **DC-DC1（MCU 供电）** 的开关，不是 ALDO1）待整体替换。
4. 视需要补第 3 节的充电器/看门狗配置项。
