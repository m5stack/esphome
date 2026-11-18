// Register values below were taken from the X-Powers AXP2101 datasheet V1.0.
// Section numbers refer to that document. Where the datasheet disagrees with
// itself the explicit code table wins, and the discrepancy is noted.

#include "axp2101.h"

namespace esphome::axp2101 {

namespace {

using axp_base::AxpAdcChannel;
using axp_base::AxpBitField;
using axp_base::AxpEvent;
using axp_base::AxpFlag;
using axp_base::AxpLutEntry;
using axp_base::AxpModel;
using axp_base::AxpNumber;
using axp_base::AxpRail;
using axp_base::AxpSelect;
using axp_base::AxpStatusBit;
using axp_base::AxpStatusFlag;
using axp_base::AxpVoltageRange;

// --- DCDC voltage windows, 6.13.2.70 to 6.13.2.74 ---------------------------

// DCDC1 is a plain 1.5 V + 100 mV * n ladder for n = 0..19 (1.5 V to 3.4 V).
constexpr AxpVoltageRange DCDC1_RANGES[] = {{1500, 100, 0, 19}};

// DCDC2 and DCDC3 share a 10 mV window up to 1.20 V, then continue in 20 mV
// steps. DCDC3 adds a third 100 mV window up to 3.4 V.
constexpr AxpVoltageRange DCDC2_RANGES[] = {
    {500, 10, 0, 70},
    {1220, 20, 71, 87},
};
constexpr AxpVoltageRange DCDC3_RANGES[] = {
    {500, 10, 0, 70},
    {1220, 20, 71, 87},
    {1600, 100, 88, 106},
};
// DCDC4 stops at 1.84 V instead of 1.54 V in its second window.
constexpr AxpVoltageRange DCDC4_RANGES[] = {
    {500, 10, 0, 70},
    {1220, 20, 71, 102},
};
constexpr AxpVoltageRange DCDC5_RANGES[] = {{1400, 100, 0, 23}};

// --- LDO voltage windows, 6.13.2.77 to 6.13.2.85 ---------------------------

constexpr AxpVoltageRange LDO_500_3500[] = {{500, 100, 0, 30}};
// 6.13.2.83 and 6.13.2.85 describe 0.5-1.4 V in 50 mV steps and label code
// 10011 (19) as 1.40 V, but 0.5 + 50 mV * 19 is 1.45 V. Code 18 is the last
// one that decodes to a voltage both readings agree on, so it is the top of the
// range: the extra step is dropped rather than risking 50 mV too much.
constexpr AxpVoltageRange LDO_500_1400[] = {{500, 50, 0, 18}};
// The DLDO1 heading claims 0.5-3.4 V over 29 steps, but the code list stops at
// 11100 = 3.3 V, which is 29 codes counting from zero.
constexpr AxpVoltageRange DLDO1_RANGES[] = {{500, 100, 0, 28}};

// --- Power outputs ---------------------------------------------------------
// Enable bits: DCDC1-5 in REG 80H, LDOs in REG 90H with DLDO2 alone in REG 91H
// (6.13.2.68, 6.13.2.75, 6.13.2.76). Load capacities are from Table 6-1.
// DCDC2 and DCDC3 are the only converters whose voltage register has the DVM
// enable in bit 7 (6.13.2.71, 6.13.2.72).

constexpr AxpRail RAILS[] = {
    {.name = "DCDC1",
     .ranges = DCDC1_RANGES,
     .max_ma = 2000,
     .range_count = 1,
     .flags = 0,
     .enable_reg = 0x80,
     .enable_mask = 0x01,
     .enable_value = 0x01,
     .vsel_reg = 0x82,
     .vsel_mask = 0x1F,
     .vsel_shift = 0},
    {.name = "DCDC2",
     .ranges = DCDC2_RANGES,
     .max_ma = 2000,
     .range_count = 2,
     .flags = axp_base::AXP_RAIL_HAS_DVM,
     .enable_reg = 0x80,
     .enable_mask = 0x02,
     .enable_value = 0x02,
     .vsel_reg = 0x83,
     .vsel_mask = 0x7F,
     .vsel_shift = 0},
    {.name = "DCDC3",
     .ranges = DCDC3_RANGES,
     .max_ma = 2000,
     .range_count = 3,
     .flags = axp_base::AXP_RAIL_HAS_DVM,
     .enable_reg = 0x80,
     .enable_mask = 0x04,
     .enable_value = 0x04,
     .vsel_reg = 0x84,
     .vsel_mask = 0x7F,
     .vsel_shift = 0},
    {.name = "DCDC4",
     .ranges = DCDC4_RANGES,
     .max_ma = 1500,
     .range_count = 2,
     .flags = 0,
     .enable_reg = 0x80,
     .enable_mask = 0x08,
     .enable_value = 0x08,
     .vsel_reg = 0x85,
     .vsel_mask = 0x7F,
     .vsel_shift = 0},
    {.name = "DCDC5",
     .ranges = DCDC5_RANGES,
     .max_ma = 1000,
     .range_count = 1,
     .flags = 0,
     .enable_reg = 0x80,
     .enable_mask = 0x10,
     .enable_value = 0x10,
     .vsel_reg = 0x86,
     .vsel_mask = 0x1F,
     .vsel_shift = 0},
    {.name = "ALDO1",
     .ranges = LDO_500_3500,
     .max_ma = 300,
     .range_count = 1,
     .flags = 0,
     .enable_reg = 0x90,
     .enable_mask = 0x01,
     .enable_value = 0x01,
     .vsel_reg = 0x92,
     .vsel_mask = 0x1F,
     .vsel_shift = 0},
    {.name = "ALDO2",
     .ranges = LDO_500_3500,
     .max_ma = 300,
     .range_count = 1,
     .flags = 0,
     .enable_reg = 0x90,
     .enable_mask = 0x02,
     .enable_value = 0x02,
     .vsel_reg = 0x93,
     .vsel_mask = 0x1F,
     .vsel_shift = 0},
    {.name = "ALDO3",
     .ranges = LDO_500_3500,
     .max_ma = 300,
     .range_count = 1,
     .flags = 0,
     .enable_reg = 0x90,
     .enable_mask = 0x04,
     .enable_value = 0x04,
     .vsel_reg = 0x94,
     .vsel_mask = 0x1F,
     .vsel_shift = 0},
    {.name = "ALDO4",
     .ranges = LDO_500_3500,
     .max_ma = 300,
     .range_count = 1,
     .flags = 0,
     .enable_reg = 0x90,
     .enable_mask = 0x08,
     .enable_value = 0x08,
     .vsel_reg = 0x95,
     .vsel_mask = 0x1F,
     .vsel_shift = 0},
    // REG 90H bit 4 is labelled "aldo1 enable" in the datasheet, repeating
    // bit 0. With ALDO1-4, BLDO1-2, CPUSLDO and DLDO1 filling bits 0-7 and
    // BLDO2 called out separately, bit 4 can only be BLDO1.
    {.name = "BLDO1",
     .ranges = LDO_500_3500,
     .max_ma = 300,
     .range_count = 1,
     .flags = 0,
     .enable_reg = 0x90,
     .enable_mask = 0x10,
     .enable_value = 0x10,
     .vsel_reg = 0x96,
     .vsel_mask = 0x1F,
     .vsel_shift = 0},
    {.name = "BLDO2",
     .ranges = LDO_500_3500,
     .max_ma = 300,
     .range_count = 1,
     .flags = 0,
     .enable_reg = 0x90,
     .enable_mask = 0x20,
     .enable_value = 0x20,
     .vsel_reg = 0x97,
     .vsel_mask = 0x1F,
     .vsel_shift = 0},
    {.name = "CPUSLDO",
     .ranges = LDO_500_1400,
     .max_ma = 30,
     .range_count = 1,
     .flags = 0,
     .enable_reg = 0x90,
     .enable_mask = 0x40,
     .enable_value = 0x40,
     .vsel_reg = 0x98,
     .vsel_mask = 0x1F,
     .vsel_shift = 0},
    {.name = "DLDO1",
     .ranges = DLDO1_RANGES,
     .max_ma = 300,
     .range_count = 1,
     .flags = 0,
     .enable_reg = 0x90,
     .enable_mask = 0x80,
     .enable_value = 0x80,
     .vsel_reg = 0x99,
     .vsel_mask = 0x1F,
     .vsel_shift = 0},
    {.name = "DLDO2",
     .ranges = LDO_500_1400,
     .max_ma = 300,
     .range_count = 1,
     .flags = 0,
     .enable_reg = 0x91,
     .enable_mask = 0x01,
     .enable_value = 0x01,
     .vsel_reg = 0x9A,
     .vsel_mask = 0x1F,
     .vsel_shift = 0},
};

// --- ADC, 6.10 and 6.13.2.29 to 6.13.2.39 ---------------------------------
// Channel enables are REG 30H bits 0-4; every channel is a 14 bit value made of
// six high bits and eight low bits, and the high register keeps its debug bits.
// `lsb` is expressed in the unit the entity publishes, so the voltage channels
// use volts per count rather than the datasheet's millivolts per count.

constexpr AxpAdcChannel ADC_CHANNELS[] = {
    {.name = "battery_voltage",
     .lsb = 0.001f,  // datasheet: 1 mV/LSB
     .offset = 0.0f,
     .enable_reg = 0x30,
     .enable_bit = 0,
     .hi_reg = 0x34,
     .hi_mask = 0x3F,
     .mid_reg = axp_base::AXP_REG_NONE,
     .lo_reg = 0x35,
     .lo_mask = 0xFF,
     .lo_bits = 8,
     .lo_shift = 0},
    {.name = "ts_voltage",
     .lsb = 0.0005f,  // datasheet: 0.5 mV/LSB
     .offset = 0.0f,
     .enable_reg = 0x30,
     .enable_bit = 1,
     .hi_reg = 0x36,
     .hi_mask = 0x3F,
     .mid_reg = axp_base::AXP_REG_NONE,
     .lo_reg = 0x37,
     .lo_mask = 0xFF,
     .lo_bits = 8,
     .lo_shift = 0},
    {.name = "vbus_voltage",
     .lsb = 0.001f,  // datasheet: 1 mV/LSB
     .offset = 0.0f,
     .enable_reg = 0x30,
     .enable_bit = 2,
     .hi_reg = 0x38,
     .hi_mask = 0x3F,
     .mid_reg = axp_base::AXP_REG_NONE,
     .lo_reg = 0x39,
     .lo_mask = 0xFF,
     .lo_bits = 8,
     .lo_shift = 0},
    {.name = "system_voltage",
     .lsb = 0.001f,  // datasheet: 1 mV/LSB
     .offset = 0.0f,
     .enable_reg = 0x30,
     .enable_bit = 3,
     .hi_reg = 0x3A,
     .hi_mask = 0x3F,
     .mid_reg = axp_base::AXP_REG_NONE,
     .lo_reg = 0x3B,
     .lo_mask = 0xFF,
     .lo_bits = 8,
     .lo_shift = 0},
    // The datasheet gives the die temperature channel 0.1 mV per count but no
    // transfer function. The conversion used by every published driver for this
    // chip is T = 22 + (7274 - raw) / 20, i.e. -2 mV per degree around 22 C.
    // Rewritten as a scale and offset for the shared ADC decoder:
    //   T = 385.7 - 0.05 * raw
    {.name = "die_temperature",
     .lsb = -0.05f,
     .offset = 385.7f,
     .enable_reg = 0x30,
     .enable_bit = 4,
     .hi_reg = 0x3C,
     .hi_mask = 0x3F,
     .mid_reg = axp_base::AXP_REG_NONE,
     .lo_reg = 0x3D,
     .lo_mask = 0xFF,
     .lo_bits = 8,
     .lo_shift = 0},
};

// --- Interrupt sources, 6.13.2.40 to 6.13.2.45 ----------------------------
// Status registers are REG 48H-4AH, enables are REG 40H-42H, so the enable
// index always equals the status index.

constexpr AxpEvent EVENTS[] = {
    {.name = "soc_warning_level", .status_index = 0, .bit = 7, .enable_index = 0, .enable_bit = 7},
    {.name = "soc_shutdown_level", .status_index = 0, .bit = 6, .enable_index = 0, .enable_bit = 6},
    {.name = "gauge_watchdog_timeout", .status_index = 0, .bit = 5, .enable_index = 0, .enable_bit = 5},
    {.name = "gauge_new_soc", .status_index = 0, .bit = 4, .enable_index = 0, .enable_bit = 4},
    {.name = "battery_over_temperature_charging", .status_index = 0, .bit = 3, .enable_index = 0, .enable_bit = 3},
    {.name = "battery_under_temperature_charging", .status_index = 0, .bit = 2, .enable_index = 0, .enable_bit = 2},
    {.name = "battery_over_temperature_working", .status_index = 0, .bit = 1, .enable_index = 0, .enable_bit = 1},
    {.name = "battery_under_temperature_working", .status_index = 0, .bit = 0, .enable_index = 0, .enable_bit = 0},
    {.name = "vbus_insert", .status_index = 1, .bit = 7, .enable_index = 1, .enable_bit = 7},
    {.name = "vbus_remove", .status_index = 1, .bit = 6, .enable_index = 1, .enable_bit = 6},
    {.name = "battery_insert", .status_index = 1, .bit = 5, .enable_index = 1, .enable_bit = 5},
    {.name = "battery_remove", .status_index = 1, .bit = 4, .enable_index = 1, .enable_bit = 4},
    {.name = "power_key_short_press", .status_index = 1, .bit = 3, .enable_index = 1, .enable_bit = 3},
    {.name = "power_key_long_press", .status_index = 1, .bit = 2, .enable_index = 1, .enable_bit = 2},
    {.name = "power_key_negative_edge", .status_index = 1, .bit = 1, .enable_index = 1, .enable_bit = 1},
    {.name = "power_key_positive_edge", .status_index = 1, .bit = 0, .enable_index = 1, .enable_bit = 0},
    {.name = "watchdog_expired", .status_index = 2, .bit = 7, .enable_index = 2, .enable_bit = 7},
    {.name = "ldo_over_current", .status_index = 2, .bit = 6, .enable_index = 2, .enable_bit = 6},
    {.name = "batfet_over_current", .status_index = 2, .bit = 5, .enable_index = 2, .enable_bit = 5},
    {.name = "charge_done", .status_index = 2, .bit = 4, .enable_index = 2, .enable_bit = 4},
    {.name = "charge_started", .status_index = 2, .bit = 3, .enable_index = 2, .enable_bit = 3},
    {.name = "die_over_temperature", .status_index = 2, .bit = 2, .enable_index = 2, .enable_bit = 2},
    {.name = "charger_safety_timer", .status_index = 2, .bit = 1, .enable_index = 2, .enable_bit = 1},
    {.name = "battery_over_voltage", .status_index = 2, .bit = 0, .enable_index = 2, .enable_bit = 0},
};

// --- Status flags, 6.13.2.1 and 6.13.2.2 ----------------------------------
// The name doubles as the configuration key, so the entity layer can address a
// flag without a second list. The chip reports only these; it has no separate
// "VBUS present" bit and reports over temperature only as an interrupt.

constexpr AxpStatusFlag STATUS_FLAGS[] = {
    {"battery_present", {0, 0x08, 0x08}},
    // REG 01H bits 6:5 select the current direction: 01 charge, 10 discharge.
    {"battery_charging", {1, 0x60, 0x20}},
    {"battery_discharging", {1, 0x60, 0x40}},
    {"vbus_good", {0, 0x20, 0x20}},
    {"battery_active_mode", {0, 0x04, 0x04}},
    {"thermal_regulation", {0, 0x02, 0x02}},
    {"current_limit", {0, 0x01, 0x01}},
};

// --- Charger, 6.13.2.59 to 6.13.2.62 --------------------------------------
// The 0 mA entry is deliberately left out: charging at zero current is the same
// as switching the charger off, which has its own bit, and keeping it would let
// a small current request snap down to no charging at all.

constexpr AxpLutEntry CHARGE_CURRENT[] = {
    {100, 4},  {125, 5},  {150, 6},  {175, 7},  {200, 8},  {300, 9},   {400, 10},
    {500, 11}, {600, 12}, {700, 13}, {800, 14}, {900, 15}, {1000, 16},
};

constexpr AxpLutEntry CHARGE_VOLTAGE[] = {
    {4000, 1}, {4100, 2}, {4200, 3}, {4350, 4}, {4400, 5},
};

// REG 16H bits 2:0 (6.13.2.12). The datasheet spells the top two as 1.5 A and
// 2 A; they are kept in mA here like every other current.
constexpr AxpLutEntry INPUT_CURRENT_LIMIT[] = {
    {100, 0}, {500, 1}, {900, 2}, {1000, 3}, {1500, 4}, {2000, 5},
};

// REG 27H bits 5:4 (6.13.2.24): how long the power key is held before it counts
// as a long press. That is the threshold the power key interrupt uses.
constexpr AxpLutEntry POWER_KEY_LONG_PRESS[] = {
    {1000, 0},
    {1500, 1},
    {2000, 2},
    {2500, 3},
};

// On/off settings. Neither is a one-shot control, so they live in their own
// table: REG 18H (6.13.2.14) holds the charger and gauge enables.
constexpr AxpFlag FLAGS[] = {
    {.name = "charging", .field = {0x18, 0x02, 1}},
    {.name = "gauge", .field = {0x18, 0x08, 3}},
    // REG 23H bits 4:0 (6.13.2.19-22, power-off behaviour): each converter powers
    // the chip off by itself when its output drops under the UVP threshold. Those
    // five bits are one group, hence `on_code`; the POR value has them all set.
    // Bit 5 (over-voltage) is deliberately left out of the mask: a rail that runs
    // away is still worth powering down for.
    {.name = "dcdc_uvp_shutdown", .field = {0x23, 0x1F, 0}, .on_code = 0x1F},
    // REG 50H bit 4 (6.13.2.46): turns the TS pin into a plain external input.
    // Left at the thermistor setting the charger regulates against whatever sits
    // on TS - on a board whose TS pin senses a rail instead of an NTC it reads a
    // permanently cold cell and never charges - and the TS ADC channel reports
    // that same rail, which is what the "is something already driving this
    // output" checks need. The power-on value comes from EFUSE, so it differs
    // between chips and always has to be written.
    {.name = "ts_external_input", .field = {0x50, 0x10, 4}},
};

// Settings on a linear scale: value = offset + step * code (6.13.2.11, 6.13.2.16,
// 6.13.2.59, 6.13.2.61). REG 1AH holds the two battery thresholds in one byte.
constexpr AxpNumber NUMBERS[] = {
    {.name = "battery_low_warning", .offset = 5, .step = 1, .field = {0x1A, 0xF0, 4}, .min_code = 0, .max_code = 15},
    {.name = "battery_low_shutdown", .offset = 0, .step = 1, .field = {0x1A, 0x0F, 0}, .min_code = 0, .max_code = 15},
    {.name = "input_voltage_limit",
     .offset = 3880,
     .step = 80,
     .field = {0x15, 0x0F, 0},
     .min_code = 0,
     .max_code = 15},
    {.name = "precharge_current", .offset = 0, .step = 25, .field = {0x61, 0x0F, 0}, .min_code = 0, .max_code = 8},
    {.name = "termination_current", .offset = 0, .step = 25, .field = {0x63, 0x0F, 0}, .min_code = 0, .max_code = 8},
};

// The enum-like settings a select can drive, in the order the value tables above
// list their options.
constexpr AxpSelect SELECTS[] = {
    {.name = "charge_current",
     .field = {.entries = CHARGE_CURRENT, .count = 13, .reg = 0x62, .mask = 0x1F, .shift = 0}},
    {.name = "charge_voltage", .field = {.entries = CHARGE_VOLTAGE, .count = 5, .reg = 0x64, .mask = 0x07, .shift = 0}},
    {.name = "input_current_limit",
     .field = {.entries = INPUT_CURRENT_LIMIT, .count = 6, .reg = 0x16, .mask = 0x07, .shift = 0}},
    {.name = "power_key_long_press",
     .field = {.entries = POWER_KEY_LONG_PRESS, .count = 4, .reg = 0x27, .mask = 0x30, .shift = 4}},
};

constexpr AxpModel MODEL = {
    .name = "AXP2101",
    .rails = RAILS,
    .adc_channels = ADC_CHANNELS,
    .events = EVENTS,
    .status_flags = STATUS_FLAGS,
    .selects = SELECTS,
    .numbers = NUMBERS,
    .flags = FLAGS,
    // REG 10H bit 0 is soft PWROFF and bit 1 restarts the SoC and re-runs the
    // power sequence (6.13.2.7). REG 17H bit 3 resets the fuel gauge (6.13.2.13).
    .power_off = {0x10, 0x01, 0},
    .restart = {0x10, 0x02, 1},
    .gauge_reset = {0x17, 0x08, 3},
    .chip_id_reg = 0x03,
    .chip_id = 0x4A,
    .status_base = 0x00,
    .status_count = 2,
    .rail_count = sizeof(RAILS) / sizeof(RAILS[0]),
    .adc_count = sizeof(ADC_CHANNELS) / sizeof(ADC_CHANNELS[0]),
    .event_count = sizeof(EVENTS) / sizeof(EVENTS[0]),
    .status_flag_count = sizeof(STATUS_FLAGS) / sizeof(STATUS_FLAGS[0]),
    .select_count = sizeof(SELECTS) / sizeof(SELECTS[0]),
    .number_count = sizeof(NUMBERS) / sizeof(NUMBERS[0]),
    .flag_count = sizeof(FLAGS) / sizeof(FLAGS[0]),
    .irq_status_base = 0x48,
    .irq_status_count = 3,
    .irq_enable_base = 0x40,
    .gauge_soc_reg = 0xA4,
};

}  // namespace

AXP2101::AXP2101() : AxpComponent(MODEL) {}

}  // namespace esphome::axp2101
