// Register values below come from the X-Powers AXP192 datasheet V1.2. Section
// numbers refer to that document. Where the datasheet disagrees with itself the
// explicit register table wins, and the discrepancy is noted at the entry.
//
// The AXP192 has no chip ID register, so `chip_id_reg` is AXP_REG_NONE and
// setup() can only check that the bus answers.

#include "axp192.h"

namespace esphome::axp192 {

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
using axp_base::AxpStatusFlag;
using axp_base::AxpVoltageRange;

// --- Voltage windows, 9.11 REG 23H/26H/27H/28H/91H -------------------------
// Three converters on a 25 mV ladder and three LDOs on a 100 mV one. The
// datasheet tops DCDC1 and DCDC3 at 3.5 V (code 112 of 128) and DCDC2 at 2.275 V
// (code 63 of 64); the codes above those are not described, so they are refused
// rather than guessed at.

constexpr AxpVoltageRange DCDC_700_3500[] = {{700, 25, 0, 112}};
constexpr AxpVoltageRange DCDC2_700_2275[] = {{700, 25, 0, 63}};
constexpr AxpVoltageRange LDO_1800_3300[] = {{1800, 100, 0, 15}};

// --- Power outputs ---------------------------------------------------------
// REG 12H is the output control register: bit 0 DCDC1, bit 1 DCDC3, bit 2 LDO2,
// bit 3 LDO3, bit 4 DCDC2, bit 6 EXTEN. Its bits 6 and 4 are the same latches as
// REG 10H bits 2 and 0 ("REG12Hbit6/4 分别对应 REG10Hbit2/0"), so the whole set is
// written through REG 12H. REG 12H bit 7 is reserved but writable, hence the
// per-rail masks instead of a whole byte.
//
// LDOIO0 has no enable bit: it is GPIO0's third function, so turning it on
// selects the LDO (010) and turning it off selects float (110) rather than code
// 0, which would leave GPIO0 driving the load as an open drain output.
//
// LDO1 is not listed. It supplies the RTC and is always on with a fixed voltage,
// so there is nothing to switch or set.

constexpr AxpRail RAILS[] = {
    {.name = "DCDC1",
     .ranges = DCDC_700_3500,
     .max_ma = 1200,
     .range_count = 1,
     .flags = 0,
     .enable_reg = 0x12,
     .enable_mask = 0x01,
     .enable_value = 0x01,
     .disable_value = 0x00,
     .vsel_reg = 0x26,
     .vsel_mask = 0x7F,
     .vsel_shift = 0},
    {.name = "DCDC2",
     .ranges = DCDC2_700_2275,
     .max_ma = 1600,
     .range_count = 1,
     .flags = 0,
     .enable_reg = 0x12,
     .enable_mask = 0x10,
     .enable_value = 0x10,
     .disable_value = 0x00,
     .vsel_reg = 0x23,
     .vsel_mask = 0x3F,
     .vsel_shift = 0},
    {.name = "DCDC3",
     .ranges = DCDC_700_3500,
     .max_ma = 700,
     .range_count = 1,
     .flags = 0,
     .enable_reg = 0x12,
     .enable_mask = 0x02,
     .enable_value = 0x02,
     .disable_value = 0x00,
     .vsel_reg = 0x27,
     .vsel_mask = 0x7F,
     .vsel_shift = 0},
    {.name = "LDO2",
     .ranges = LDO_1800_3300,
     .max_ma = 200,
     .range_count = 1,
     .flags = 0,
     .enable_reg = 0x12,
     .enable_mask = 0x04,
     .enable_value = 0x04,
     .disable_value = 0x00,
     .vsel_reg = 0x28,
     .vsel_mask = 0xF0,
     .vsel_shift = 4},
    {.name = "LDO3",
     .ranges = LDO_1800_3300,
     .max_ma = 200,
     .range_count = 1,
     .flags = 0,
     .enable_reg = 0x12,
     .enable_mask = 0x08,
     .enable_value = 0x08,
     .disable_value = 0x00,
     .vsel_reg = 0x28,
     .vsel_mask = 0x0F,
     .vsel_shift = 0},
    // GPIO0's function field selects the low noise LDO (010) or float (110);
    // the remaining functions would leave the pin driving the load.
    {.name = "LDOIO0",
     .ranges = LDO_1800_3300,
     .max_ma = 50,
     .range_count = 1,
     .flags = 0,
     .enable_reg = 0x90,
     .enable_mask = 0x07,
     .enable_value = 0x02,
     .disable_value = 0x06,
     .vsel_reg = 0x91,
     .vsel_mask = 0xF0,
     .vsel_shift = 4},
    // EXTEN is the chip's switched path to an external regulator: it has a
    // switch but no voltage of its own.
    {.name = "EXTEN",
     .ranges = nullptr,
     .max_ma = 0,
     .range_count = 0,
     .flags = 0,
     .enable_reg = 0x12,
     .enable_mask = 0x40,
     .enable_value = 0x40,
     .disable_value = 0x00,
     .vsel_reg = axp_base::AXP_REG_NONE,
     .vsel_mask = 0x00,
     .vsel_shift = 0},
};

// --- ADC, 9.7 and 9.11 ADC data registers ----------------------------------
// `lsb` is expressed in the unit the entity publishes, so the voltage channels
// use volts per count rather than the datasheet's millivolts per count. The
// battery, ACIN, VBUS, temperature, TS, APS and GPIO channels are 12 bits in a
// high 8 bit register plus the bottom 4 bits of the next one; the charge and
// discharge currents are 13 bits, so their low part uses the bottom 5 bits.
//
// The GPIO channels are listed for completeness and are readable through
// read_adc(), but they are not exposed as sensors: measuring one needs the pin
// switched to the ADC function, which the pin platform owns.

constexpr AxpAdcChannel ADC_CHANNELS[] = {
    {.name = "battery_voltage",
     .lsb = 0.0011f,  // 1.1 mV/LSB
     .offset = 0.0f,
     .enable_reg = 0x82,
     .enable_bit = 7,
     .hi_reg = 0x78,
     .hi_mask = 0xFF,
     .mid_reg = axp_base::AXP_REG_NONE,
     .lo_reg = 0x79,
     .lo_mask = 0x0F,
     .lo_bits = 4,
     .lo_shift = 0},
    {.name = "battery_charge_current",
     .lsb = 0.0005f,  // 0.5 mA/LSB
     .offset = 0.0f,
     .enable_reg = 0x82,
     .enable_bit = 6,
     .hi_reg = 0x7A,
     .hi_mask = 0xFF,
     .mid_reg = axp_base::AXP_REG_NONE,
     .lo_reg = 0x7B,
     .lo_mask = 0x1F,
     .lo_bits = 5,
     .lo_shift = 0},
    // The charge and discharge currents share one enable bit, which is what the
    // chip offers; switching either channel on switches both.
    {.name = "battery_discharge_current",
     .lsb = 0.0005f,  // 0.5 mA/LSB
     .offset = 0.0f,
     .enable_reg = 0x82,
     .enable_bit = 6,
     .hi_reg = 0x7C,
     .hi_mask = 0xFF,
     .mid_reg = axp_base::AXP_REG_NONE,
     .lo_reg = 0x7D,
     .lo_mask = 0x1F,
     .lo_bits = 5,
     .lo_shift = 0},
    // Pbat = raw * 1.1 mV * 0.5 mA / 1000, published in watts so the entity uses
    // the same unit as any other power sensor: 1.1 * 0.5 / 1000 / 1000 W per
    // count. Power needs the voltage and current converters; only the current
    // one is off by default.
    {.name = "battery_power",
     .lsb = 0.00000055f,
     .offset = 0.0f,
     .enable_reg = 0x82,
     .enable_bit = 6,
     .hi_reg = 0x70,
     .hi_mask = 0xFF,
     .mid_reg = 0x71,
     .lo_reg = 0x72,
     .lo_mask = 0xFF,
     .lo_bits = 8,
     .lo_shift = 0},
    {.name = "acin_voltage",
     .lsb = 0.0017f,  // 1.7 mV/LSB
     .offset = 0.0f,
     .enable_reg = 0x82,
     .enable_bit = 5,
     .hi_reg = 0x56,
     .hi_mask = 0xFF,
     .mid_reg = axp_base::AXP_REG_NONE,
     .lo_reg = 0x57,
     .lo_mask = 0x0F,
     .lo_bits = 4,
     .lo_shift = 0},
    {.name = "acin_current",
     .lsb = 0.000625f,  // 0.625 mA/LSB
     .offset = 0.0f,
     .enable_reg = 0x82,
     .enable_bit = 4,
     .hi_reg = 0x58,
     .hi_mask = 0xFF,
     .mid_reg = axp_base::AXP_REG_NONE,
     .lo_reg = 0x59,
     .lo_mask = 0x0F,
     .lo_bits = 4,
     .lo_shift = 0},
    {.name = "vbus_voltage",
     .lsb = 0.0017f,  // 1.7 mV/LSB
     .offset = 0.0f,
     .enable_reg = 0x82,
     .enable_bit = 3,
     .hi_reg = 0x5A,
     .hi_mask = 0xFF,
     .mid_reg = axp_base::AXP_REG_NONE,
     .lo_reg = 0x5B,
     .lo_mask = 0x0F,
     .lo_bits = 4,
     .lo_shift = 0},
    {.name = "vbus_current",
     .lsb = 0.000375f,  // 0.375 mA/LSB
     .offset = 0.0f,
     .enable_reg = 0x82,
     .enable_bit = 2,
     .hi_reg = 0x5C,
     .hi_mask = 0xFF,
     .mid_reg = axp_base::AXP_REG_NONE,
     .lo_reg = 0x5D,
     .lo_mask = 0x0F,
     .lo_bits = 4,
     .lo_shift = 0},
    {.name = "aps_voltage",
     .lsb = 0.0014f,  // 1.4 mV/LSB
     .offset = 0.0f,
     .enable_reg = 0x82,
     .enable_bit = 1,
     .hi_reg = 0x7E,
     .hi_mask = 0xFF,
     .mid_reg = axp_base::AXP_REG_NONE,
     .lo_reg = 0x7F,
     .lo_mask = 0x0F,
     .lo_bits = 4,
     .lo_shift = 0},
    {.name = "ts_voltage",
     .lsb = 0.0008f,  // 0.8 mV/LSB
     .offset = 0.0f,
     .enable_reg = 0x82,
     .enable_bit = 0,
     .hi_reg = 0x62,
     .hi_mask = 0xFF,
     .mid_reg = axp_base::AXP_REG_NONE,
     .lo_reg = 0x63,
     .lo_mask = 0x0F,
     .lo_bits = 4,
     .lo_shift = 0},
    // 0.1 C per count starting at -144.7 C, which is the datasheet's own table.
    {.name = "die_temperature",
     .lsb = 0.1f,
     .offset = -144.7f,
     .enable_reg = 0x83,
     .enable_bit = 7,
     .hi_reg = 0x5E,
     .hi_mask = 0xFF,
     .mid_reg = axp_base::AXP_REG_NONE,
     .lo_reg = 0x5F,
     .lo_mask = 0x0F,
     .lo_bits = 4,
     .lo_shift = 0},
    // GPIO0-3 measure 0-2.0475 V or 0.7-2.7475 V, selected per pin in REG 85H.
    // The table holds the power-on setting, which is the 0 V floor for all four.
    {.name = "gpio0_voltage",
     .lsb = 0.0005f,  // 0.5 mV/LSB
     .offset = 0.0f,
     .enable_reg = 0x83,
     .enable_bit = 3,
     .hi_reg = 0x64,
     .hi_mask = 0xFF,
     .mid_reg = axp_base::AXP_REG_NONE,
     .lo_reg = 0x65,
     .lo_mask = 0x0F,
     .lo_bits = 4,
     .lo_shift = 0},
    {.name = "gpio1_voltage",
     .lsb = 0.0005f,
     .offset = 0.0f,
     .enable_reg = 0x83,
     .enable_bit = 2,
     .hi_reg = 0x66,
     .hi_mask = 0xFF,
     .mid_reg = axp_base::AXP_REG_NONE,
     .lo_reg = 0x67,
     .lo_mask = 0x0F,
     .lo_bits = 4,
     .lo_shift = 0},
    {.name = "gpio2_voltage",
     .lsb = 0.0005f,
     .offset = 0.0f,
     .enable_reg = 0x83,
     .enable_bit = 1,
     .hi_reg = 0x68,
     .hi_mask = 0xFF,
     .mid_reg = axp_base::AXP_REG_NONE,
     .lo_reg = 0x69,
     .lo_mask = 0x0F,
     .lo_bits = 4,
     .lo_shift = 0},
    {.name = "gpio3_voltage",
     .lsb = 0.0005f,
     .offset = 0.0f,
     .enable_reg = 0x83,
     .enable_bit = 0,
     .hi_reg = 0x6A,
     .hi_mask = 0xFF,
     .mid_reg = axp_base::AXP_REG_NONE,
     .lo_reg = 0x6B,
     .lo_mask = 0x0F,
     .lo_bits = 4,
     .lo_shift = 0},
};

// --- Status flags, REG 00H and REG 01H ------------------------------------
// The name doubles as the configuration key. REG 04H (OTG VBUS session status)
// is not cached: the hub reads one contiguous block and 04H does not join 00H
// and 01H, so those three bits are left to the vbus_* events.

constexpr AxpStatusFlag STATUS_FLAGS[] = {
    {"acin_present", {0, 0x80, 0x80}},
    {"acin_good", {0, 0x40, 0x40}},
    {"vbus_present", {0, 0x20, 0x20}},
    {"vbus_good", {0, 0x10, 0x10}},
    {"vbus_above_vhold", {0, 0x08, 0x08}},
    // REG 00H bit 2 is the current direction, not a charging flag: 1 while the
    // battery takes current, 0 while it supplies the system.
    {"battery_charging", {1, 0x40, 0x40}},
    {"battery_discharging", {0, 0x04, 0x00}},
    {"acin_vbus_shorted", {0, 0x02, 0x02}},
    {"boot_from_acin_vbus", {0, 0x01, 0x01}},
    {"over_temperature", {1, 0x80, 0x80}},
    {"battery_present", {1, 0x20, 0x20}},
    {"battery_active_mode", {1, 0x08, 0x08}},
    {"charge_current_limited", {1, 0x04, 0x04}},
    {"power_on_mode_b", {1, 0x02, 0x02}},
};

// --- Interrupt sources, REG 44H to REG 47H --------------------------------
// Status registers are REG 44H-47H and enables are REG 40H-43H, so the enable
// index always equals the status index. REG 4DH (IRQ5: the internal timer and
// the GPIO0-2 edge detectors) is not modelled because it does not join that
// block; the timer and the pin edges are described in the README instead.

constexpr AxpEvent EVENTS[] = {
    {.name = "acin_over_voltage", .status_index = 0, .bit = 7, .enable_index = 0, .enable_bit = 7},
    {.name = "acin_insert", .status_index = 0, .bit = 6, .enable_index = 0, .enable_bit = 6},
    {.name = "acin_remove", .status_index = 0, .bit = 5, .enable_index = 0, .enable_bit = 5},
    {.name = "vbus_over_voltage", .status_index = 0, .bit = 4, .enable_index = 0, .enable_bit = 4},
    {.name = "vbus_insert", .status_index = 0, .bit = 3, .enable_index = 0, .enable_bit = 3},
    {.name = "vbus_remove", .status_index = 0, .bit = 2, .enable_index = 0, .enable_bit = 2},
    {.name = "vbus_below_vhold", .status_index = 0, .bit = 1, .enable_index = 0, .enable_bit = 1},
    {.name = "battery_insert", .status_index = 1, .bit = 7, .enable_index = 1, .enable_bit = 7},
    {.name = "battery_remove", .status_index = 1, .bit = 6, .enable_index = 1, .enable_bit = 6},
    {.name = "battery_enter_active_mode", .status_index = 1, .bit = 5, .enable_index = 1, .enable_bit = 5},
    {.name = "battery_exit_active_mode", .status_index = 1, .bit = 4, .enable_index = 1, .enable_bit = 4},
    {.name = "battery_charge_started", .status_index = 1, .bit = 3, .enable_index = 1, .enable_bit = 3},
    {.name = "battery_charge_done", .status_index = 1, .bit = 2, .enable_index = 1, .enable_bit = 2},
    {.name = "battery_over_temperature", .status_index = 1, .bit = 1, .enable_index = 1, .enable_bit = 1},
    {.name = "battery_under_temperature", .status_index = 1, .bit = 0, .enable_index = 1, .enable_bit = 0},
    {.name = "die_over_temperature", .status_index = 2, .bit = 7, .enable_index = 2, .enable_bit = 7},
    {.name = "charge_current_below_setting", .status_index = 2, .bit = 6, .enable_index = 2, .enable_bit = 6},
    {.name = "dcdc1_under_voltage", .status_index = 2, .bit = 5, .enable_index = 2, .enable_bit = 5},
    {.name = "dcdc2_under_voltage", .status_index = 2, .bit = 4, .enable_index = 2, .enable_bit = 4},
    {.name = "dcdc3_under_voltage", .status_index = 2, .bit = 3, .enable_index = 2, .enable_bit = 3},
    {.name = "power_key_short_press", .status_index = 2, .bit = 1, .enable_index = 2, .enable_bit = 1},
    {.name = "power_key_long_press", .status_index = 2, .bit = 0, .enable_index = 2, .enable_bit = 0},
    {.name = "n_oe_power_on", .status_index = 3, .bit = 7, .enable_index = 3, .enable_bit = 7},
    {.name = "n_oe_power_off", .status_index = 3, .bit = 6, .enable_index = 3, .enable_bit = 6},
    {.name = "vbus_valid", .status_index = 3, .bit = 5, .enable_index = 3, .enable_bit = 5},
    {.name = "vbus_invalid", .status_index = 3, .bit = 4, .enable_index = 3, .enable_bit = 4},
    {.name = "vbus_session_valid", .status_index = 3, .bit = 3, .enable_index = 3, .enable_bit = 3},
    {.name = "vbus_session_end", .status_index = 3, .bit = 2, .enable_index = 3, .enable_bit = 2},
    {.name = "aps_low_voltage", .status_index = 3, .bit = 0, .enable_index = 3, .enable_bit = 0},
};

// --- Charger, REG 30H/33H/34H/35H/8BH -------------------------------------
// The current table is the whole 4 bit field; unlike the AXP2101 there is no
// "no charging" code in it, so every entry is a real current and a small request
// snaps to 100 mA.

constexpr AxpLutEntry CHARGE_CURRENT[] = {
    {100, 0}, {190, 1}, {280, 2},  {360, 3},   {450, 4},   {550, 5},   {630, 6},   {700, 7},
    {780, 8}, {880, 9}, {960, 10}, {1000, 11}, {1080, 12}, {1160, 13}, {1240, 14}, {1320, 15},
};

constexpr AxpLutEntry CHARGE_VOLTAGE[] = {
    {4100, 0},
    {4150, 1},
    {4200, 2},
    {4360, 3},
};

// REG 33H bit 4: end of charge at 10 % or 15 % of the programmed current.
constexpr AxpLutEntry CHARGE_TERMINATION[] = {
    {10, 0},
    {15, 1},
};

// REG 30H bits 1:0. Code 3 is the only one that switches the limit on at
// 100 mA; code 0 leaves the input unlimited.
constexpr AxpLutEntry VBUS_CURRENT_LIMIT[] = {
    {0, 0},
    {500, 1},
    {100, 3},
};

// REG 30H bits 5:3, the voltage VBUS is held down to before it is used.
constexpr AxpLutEntry VBUS_VHOLD[] = {
    {4000, 0}, {4100, 1}, {4200, 2}, {4300, 3}, {4400, 4}, {4500, 5}, {4600, 6}, {4700, 7},
};

// REG 34H bits 7:6 and bits 1:0.
constexpr AxpLutEntry PRECHARGE_TIMEOUT[] = {
    {30, 0},
    {40, 1},
    {50, 2},
    {60, 3},
};
constexpr AxpLutEntry CONSTANT_CURRENT_TIMEOUT[] = {
    {7, 0},
    {8, 1},
    {9, 2},
    {10, 3},
};

// REG 36H, the power key: bits 7:6 how long it has to be held to switch the chip
// on, bits 5:4 when it counts as a long press, bits 1:0 how long it has to be
// held to switch the chip off.
constexpr AxpLutEntry POWER_KEY_ON_TIME[] = {
    {128, 0},
    {512, 1},
    {1000, 2},
    {2000, 3},
};
constexpr AxpLutEntry POWER_KEY_LONG_PRESS[] = {
    {1000, 0},
    {1500, 1},
    {2000, 2},
    {2500, 3},
};
constexpr AxpLutEntry POWER_KEY_OFF_TIME[] = {
    {4000, 0},
    {6000, 1},
    {8000, 2},
    {10000, 3},
};

// REG 35H. Codes 1 and 2 both mean 3.0 V in the datasheet; only code 1 is
// listed so the setting never shows two identical options, and a chip sitting on
// code 2 reads back as an unknown code rather than a duplicate.
constexpr AxpLutEntry BACKUP_BATTERY_VOLTAGE[] = {
    {3100, 0},
    {3000, 1},
    {2500, 3},
};
constexpr AxpLutEntry BACKUP_BATTERY_CURRENT[] = {
    {50, 0},
    {100, 1},
    {200, 2},
    {400, 3},
};

// REG 8BH bits 5:4.
constexpr AxpLutEntry VBUS_VALID_VOLTAGE[] = {
    {4000, 0},
    {4150, 1},
    {4450, 2},
    {4550, 3},
};

constexpr AxpSelect SELECTS[] = {
    {.name = "charge_current",
     .field = {.entries = CHARGE_CURRENT, .count = 16, .reg = 0x33, .mask = 0x0F, .shift = 0}},
    {.name = "charge_voltage", .field = {.entries = CHARGE_VOLTAGE, .count = 4, .reg = 0x33, .mask = 0x60, .shift = 5}},
    {.name = "charge_termination_current",
     .field = {.entries = CHARGE_TERMINATION, .count = 2, .reg = 0x33, .mask = 0x10, .shift = 4}},
    {.name = "vbus_current_limit",
     .field = {.entries = VBUS_CURRENT_LIMIT, .count = 3, .reg = 0x30, .mask = 0x03, .shift = 0}},
    {.name = "vbus_vhold", .field = {.entries = VBUS_VHOLD, .count = 8, .reg = 0x30, .mask = 0x38, .shift = 3}},
    {.name = "precharge_timeout",
     .field = {.entries = PRECHARGE_TIMEOUT, .count = 4, .reg = 0x34, .mask = 0xC0, .shift = 6}},
    {.name = "constant_current_timeout",
     .field = {.entries = CONSTANT_CURRENT_TIMEOUT, .count = 4, .reg = 0x34, .mask = 0x03, .shift = 0}},
    {.name = "power_key_on_time",
     .field = {.entries = POWER_KEY_ON_TIME, .count = 4, .reg = 0x36, .mask = 0xC0, .shift = 6}},
    {.name = "power_key_long_press",
     .field = {.entries = POWER_KEY_LONG_PRESS, .count = 4, .reg = 0x36, .mask = 0x30, .shift = 4}},
    {.name = "power_key_off_time",
     .field = {.entries = POWER_KEY_OFF_TIME, .count = 4, .reg = 0x36, .mask = 0x03, .shift = 0}},
    {.name = "backup_battery_voltage",
     .field = {.entries = BACKUP_BATTERY_VOLTAGE, .count = 3, .reg = 0x35, .mask = 0x60, .shift = 5}},
    {.name = "backup_battery_current",
     .field = {.entries = BACKUP_BATTERY_CURRENT, .count = 4, .reg = 0x35, .mask = 0x03, .shift = 0}},
    {.name = "vbus_valid_voltage",
     .field = {.entries = VBUS_VALID_VOLTAGE, .count = 4, .reg = 0x8B, .mask = 0x30, .shift = 4}},
};

// --- Settings on a linear scale -------------------------------------------
// Value = offset + step * code. VOFF is REG 31H bits 2:0, the GPIO1 ADC
// interrupt thresholds are REG 86H/87H at 8 mV per count, and the internal timer
// counts minutes in REG 8AH bits 6:0 (0 leaves it off).

constexpr AxpNumber NUMBERS[] = {
    {.name = "voff_voltage", .offset = 2600, .step = 100, .field = {0x31, 0x07, 0}, .min_code = 0, .max_code = 7},
    {.name = "gpio1_adc_irq_rise", .offset = 0, .step = 8, .field = {0x86, 0xFF, 0}, .min_code = 0, .max_code = 255},
    {.name = "gpio1_adc_irq_fall", .offset = 0, .step = 8, .field = {0x87, 0xFF, 0}, .min_code = 0, .max_code = 255},
    {.name = "timer_minutes", .offset = 0, .step = 1, .field = {0x8A, 0x7F, 0}, .min_code = 0, .max_code = 127},
};

// --- On/off settings ------------------------------------------------------
// The first two are the ones set_charging_enabled() and set_gauge_enabled()
// drive; the rest are the chip's other single bit switches.

constexpr AxpFlag FLAGS[] = {
    // REG 33H bit 7 switches the charger, including its external channel.
    {.name = "charging", .field = {0x33, 0x80, 7}},
    // REG B8H bit 7 runs the coulomb counter. Without it the counters in REG
    // B0H-B7H stop moving, so there is nothing to integrate charge from.
    {.name = "gauge", .field = {0xB8, 0x80, 7}},
    {.name = "battery_monitor", .field = {0x32, 0x40, 6}},
    {.name = "over_temperature_shutdown", .field = {0x8F, 0x04, 2}},
    {.name = "vbus_vhold_enable", .field = {0x30, 0x40, 6}},
    {.name = "vbus_ignore_n_vbusen", .field = {0x30, 0x80, 7}},
    {.name = "backup_battery_charging", .field = {0x35, 0x80, 7}},
    {.name = "charge_external_channel", .field = {0x34, 0x04, 2}},
    {.name = "dcdc1_force_pwm", .field = {0x80, 0x08, 3}},
    {.name = "dcdc2_force_pwm", .field = {0x80, 0x04, 2}},
    {.name = "dcdc3_force_pwm", .field = {0x80, 0x02, 1}},
};

constexpr AxpModel MODEL = {
    .name = "AXP192",
    .rails = RAILS,
    .adc_channels = ADC_CHANNELS,
    .events = EVENTS,
    .status_flags = STATUS_FLAGS,
    .selects = SELECTS,
    .numbers = NUMBERS,
    .flags = FLAGS,
    // REG 32H bit 7 removes the chip's outputs. The AXP192 has no register that
    // restarts the SoC, so `restart` stays unset and the button is not offered.
    // REG B8H bit 5 clears the coulomb counters and clears itself again.
    .power_off = {0x32, 0x80, 7},
    .restart = {axp_base::AXP_REG_NONE, 0, 0},
    .gauge_reset = {0xB8, 0x20, 5},
    .chip_id_reg = axp_base::AXP_REG_NONE,
    .chip_id = 0x00,
    .status_base = 0x00,
    .status_count = 2,
    .rail_count = sizeof(RAILS) / sizeof(RAILS[0]),
    .adc_count = sizeof(ADC_CHANNELS) / sizeof(ADC_CHANNELS[0]),
    .event_count = sizeof(EVENTS) / sizeof(EVENTS[0]),
    .status_flag_count = sizeof(STATUS_FLAGS) / sizeof(STATUS_FLAGS[0]),
    .select_count = sizeof(SELECTS) / sizeof(SELECTS[0]),
    .number_count = sizeof(NUMBERS) / sizeof(NUMBERS[0]),
    .flag_count = sizeof(FLAGS) / sizeof(FLAGS[0]),
    .irq_status_base = 0x44,
    .irq_status_count = 4,
    .irq_enable_base = 0x40,
    // The AXP192 counts coulombs in REG B0H-B7H but has no state of charge
    // register, so there is no battery percentage to publish.
    .gauge_soc_reg = axp_base::AXP_REG_NONE,
};

// --- GPIO ---------------------------------------------------------------
// GPIO0-2 share a layout: a three bit function field in REG 90H/92H/93H, the
// level in REG 94H, and a pull-down switch in REG 97H. GPIO3 and GPIO4 switch
// between four functions in REG 95H, but only once its bit 7 selects GPIO mode,
// and their value lives in REG 96H.

/// GPIO0 to GPIO4 are free; GPIO5 shares the N_RSTO reset line and is left out.
/// `GPIO_PIN_COUNT` cannot be used here: the ESP-IDF headers define it as a
/// macro for the SoC's own pin count.
constexpr uint8_t AXP192_GPIO_COUNT = 5;

constexpr uint8_t GPIO012_FUNCTION_REG[] = {0x90, 0x92, 0x93};
constexpr uint8_t GPIO012_LEVEL_REG = 0x94;
constexpr uint8_t GPIO34_FUNCTION_REG = 0x95;
constexpr uint8_t GPIO34_LEVEL_REG = 0x96;
constexpr uint8_t GPIO012_PULLDOWN_REG = 0x97;

/// GPIO012 function field: open drain output, or plain input.
constexpr uint8_t GPIO012_FUNCTION_OPEN_DRAIN = 0x00;
constexpr uint8_t GPIO012_FUNCTION_INPUT = 0x01;
/// GPIO34 function field: open drain output, or plain input.
constexpr uint8_t GPIO34_FUNCTION_OPEN_DRAIN = 0x01;
constexpr uint8_t GPIO34_FUNCTION_INPUT = 0x02;
/// REG 95H bit 7 hands GPIO3 and GPIO4 to the GPIO block at all.
constexpr uint8_t GPIO34_ENABLE = 0x80;

const LogString *gpio012_function_name(uint8_t pin, uint8_t code) {
  switch (code) {
    case 0x00:
      return LOG_STR("open drain output");
    case 0x01:
      return LOG_STR("input");
    case 0x02:
      if (pin == 0)
        return LOG_STR("LDO");
      return pin == 1 ? LOG_STR("PWM1") : LOG_STR("PWM2");
    case 0x03:
      return LOG_STR("reserved");
    case 0x04:
      return LOG_STR("ADC input");
    case 0x05:
      return LOG_STR("output low");
    default:
      return LOG_STR("float");
  }
}

const LogString *gpio34_function_name(uint8_t code) {
  switch (code) {
    case 0x00:
      return LOG_STR("external charge control");
    case 0x01:
      return LOG_STR("open drain output");
    case 0x02:
      return LOG_STR("input");
    default:
      return LOG_STR("reserved");
  }
}

}  // namespace

ESPHOME_LOG_TAG(TAG, "axp192");

AXP192::AXP192() : AxpComponent(MODEL) {}

void AXP192::pin_mode(uint8_t pin, gpio::Flags flags) {
  const bool input = (flags & gpio::FLAG_INPUT) != gpio::FLAG_NONE;
  const bool pulldown = (flags & gpio::FLAG_PULLDOWN) != gpio::FLAG_NONE;

  if (pin < 3) {
    const uint8_t bit = static_cast<uint8_t>(1u << pin);
    if (pin == 0 && !input) {
      // GPIO0 and LDOIO0 are the same pin, so an output here overrides the rail.
      ESP_LOGD(TAG, "GPIO0 is switched to an open drain output, LDOIO0 is off");
    }
    if (!this->update_reg_(GPIO012_FUNCTION_REG[pin], 0x07,
                           input ? GPIO012_FUNCTION_INPUT : GPIO012_FUNCTION_OPEN_DRAIN)) {
      return;
    }
    this->update_reg_(GPIO012_PULLDOWN_REG, bit, pulldown ? bit : 0);
    return;
  }

  if (pin < AXP192_GPIO_COUNT) {
    // GPIO3 occupies bits 1:0 of REG 95H and GPIO4 bits 3:2.
    const uint8_t shift = pin == 3 ? 0 : 2;
    const uint8_t code = input ? GPIO34_FUNCTION_INPUT : GPIO34_FUNCTION_OPEN_DRAIN;
    this->update_reg_(GPIO34_FUNCTION_REG, static_cast<uint8_t>(GPIO34_ENABLE | (0x03 << shift)),
                      static_cast<uint8_t>(GPIO34_ENABLE | (code << shift)));
    return;
  }

  ESP_LOGW(TAG, "GPIO%u is not a free GPIO of this chip", static_cast<unsigned>(pin));
}

bool AXP192::digital_read(uint8_t pin) {
  uint8_t reg;
  uint8_t bit;
  if (pin < 3) {
    reg = GPIO012_LEVEL_REG;
    bit = static_cast<uint8_t>(4 + pin);
  } else if (pin < AXP192_GPIO_COUNT) {
    reg = GPIO34_LEVEL_REG;
    bit = static_cast<uint8_t>(4 + (pin - 3));
  } else {
    ESP_LOGW(TAG, "GPIO%u is not a free GPIO of this chip", static_cast<unsigned>(pin));
    return false;
  }

  uint8_t value = 0;
  if (!this->read_reg_(reg, value)) {
    return false;
  }
  return (value & (1u << bit)) != 0;
}

void AXP192::digital_write(uint8_t pin, bool value) {
  uint8_t reg;
  uint8_t bit;
  if (pin < 3) {
    reg = GPIO012_LEVEL_REG;
    bit = pin;
  } else if (pin < AXP192_GPIO_COUNT) {
    reg = GPIO34_LEVEL_REG;
    bit = static_cast<uint8_t>(pin - 3);
  } else {
    ESP_LOGW(TAG, "GPIO%u is not a free GPIO of this chip", static_cast<unsigned>(pin));
    return;
  }

  // The outputs are open drain, so a one releases the pin to its pull-up and a
  // zero pulls it down through the internal NMOS.
  const uint8_t mask = static_cast<uint8_t>(1u << bit);
  this->update_reg_(reg, mask, value ? mask : 0);
}

void AXP192::dump_config() {
  AxpComponent::dump_config();

  uint8_t levels012 = 0;
  uint8_t levels34 = 0;
  uint8_t function_reg = 0;
  if (!this->read_reg_(GPIO012_LEVEL_REG, levels012) || !this->read_reg_(GPIO34_LEVEL_REG, levels34) ||
      !this->read_reg_(GPIO34_FUNCTION_REG, function_reg)) {
    return;
  }

  for (uint8_t pin = 0; pin < AXP192_GPIO_COUNT; pin++) {
    const bool high = pin < 3 ? (levels012 & (1u << (4 + pin))) != 0 : (levels34 & (1u << (4 + (pin - 3)))) != 0;
    const LogString *level = high ? LOG_STR("high") : LOG_STR("low");

    uint8_t function = 0;
    if (pin < 3) {
      if (!this->read_reg_(GPIO012_FUNCTION_REG[pin], function)) {
        return;
      }
      ESP_LOGCONFIG(TAG, "  GPIO%u: %s, reads %s", static_cast<unsigned>(pin),
                    LOG_STR_ARG(gpio012_function_name(pin, function & 0x07)), LOG_STR_ARG(level));
      continue;
    }

    function = static_cast<uint8_t>((function_reg >> (pin == 3 ? 0 : 2)) & 0x03);
    ESP_LOGCONFIG(TAG, "  GPIO%u: %s%s, reads %s", static_cast<unsigned>(pin),
                  (function_reg & GPIO34_ENABLE) != 0 ? LOG_STR("") : LOG_STR("not in GPIO mode, "),
                  LOG_STR_ARG(gpio34_function_name(function)), LOG_STR_ARG(level));
  }
}

}  // namespace esphome::axp192

void esphome::axp192::AXP192GPIOPin::setup() { this->pin_mode(this->flags_); }

void esphome::axp192::AXP192GPIOPin::pin_mode(gpio::Flags flags) {
  if (this->parent_ != nullptr)
    this->parent_->pin_mode(this->pin_, flags);
}

bool esphome::axp192::AXP192GPIOPin::digital_read() {
  return this->parent_ != nullptr && this->parent_->digital_read(this->pin_) != this->inverted_;
}

void esphome::axp192::AXP192GPIOPin::digital_write(bool value) {
  if (this->parent_ != nullptr)
    this->parent_->digital_write(this->pin_, value != this->inverted_);
}

size_t esphome::axp192::AXP192GPIOPin::dump_summary(char *buffer, size_t len) const {
  return buf_append_printf(buffer, len, 0, "%u via AXP192", this->pin_);
}
