#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "esphome/components/axp_base/axp_base.h"

namespace esphome::axp_base::testing {

namespace {

constexpr uint8_t ADDRESS = 0x34;

// Stand-in chip that exercises every shape the shared code has to cope with:
// a single voltage window, a rail with three windows, a rail without voltage
// control, a rail without a switch, and an always-on rail.
constexpr AxpVoltageRange DCDC1_RANGES[] = {{700, 25, 0, 112}};
constexpr AxpVoltageRange DCDC3_RANGES[] = {
    {500, 10, 0, 70},
    {1220, 20, 71, 87},
    {1600, 100, 88, 106},
};
constexpr AxpVoltageRange ALDO1_RANGES[] = {{500, 100, 0, 30}};

constexpr AxpRail RAILS[] = {
    {.name = "DCDC1",
     .ranges = DCDC1_RANGES,
     .max_ma = 1200,
     .range_count = 1,
     .flags = AXP_RAIL_ALWAYS_ON,
     .enable_reg = 0x12,
     .enable_mask = 0x01,
     .enable_value = 0x01,
     .vsel_reg = 0x26,
     .vsel_mask = 0x7F,
     .vsel_shift = 0},
    {.name = "DCDC3",
     .ranges = DCDC3_RANGES,
     .max_ma = 2000,
     .range_count = 3,
     .flags = AXP_RAIL_HAS_DVM,
     .enable_reg = 0x12,
     .enable_mask = 0x02,
     .enable_value = 0x02,
     .vsel_reg = 0x27,
     .vsel_mask = 0x7F,
     .vsel_shift = 0},
    {.name = "ALDO1",
     .ranges = ALDO1_RANGES,
     .max_ma = 300,
     .range_count = 1,
     .flags = 0,
     .enable_reg = 0x90,
     .enable_mask = 0x01,
     .enable_value = 0x01,
     .vsel_reg = 0x92,
     .vsel_mask = 0x1F,
     .vsel_shift = 0},
    {.name = "EXTEN",
     .ranges = nullptr,
     .max_ma = 0,
     .range_count = 0,
     .flags = 0,
     .enable_reg = 0x10,
     .enable_mask = 0x04,
     .enable_value = 0x04,
     .vsel_reg = AXP_REG_NONE,
     .vsel_mask = 0,
     .vsel_shift = 0},
    {.name = "LDO1",
     .ranges = nullptr,
     .max_ma = 0,
     .range_count = 0,
     .flags = 0,
     .enable_reg = AXP_REG_NONE,
     .enable_mask = 0,
     .enable_value = 0,
     .vsel_reg = AXP_REG_NONE,
     .vsel_mask = 0,
     .vsel_shift = 0},
};

constexpr AxpAdcChannel ADC_CHANNELS[] = {
    {.name = "battery_voltage",
     .lsb = 1.1f,
     .offset = 0.0f,
     .enable_reg = 0x82,
     .enable_bit = 0,
     .hi_reg = 0x78,
     .hi_mask = 0xFF,
     .mid_reg = AXP_REG_NONE,
     .lo_reg = 0x79,
     .lo_mask = 0xF0,
     .lo_bits = 4},
    {.name = "die_temperature",
     .lsb = 0.1f,
     .offset = -144.7f,
     .enable_reg = 0x82,
     .enable_bit = 4,
     .hi_reg = 0x3C,
     .hi_mask = 0x3F,
     .mid_reg = AXP_REG_NONE,
     .lo_reg = 0x3D,
     .lo_mask = 0xFF,
     .lo_bits = 8},
    {.name = "battery_power",
     .lsb = 0.5f,
     .offset = 0.0f,
     .enable_reg = AXP_REG_NONE,
     .enable_bit = 0,
     .hi_reg = 0x70,
     .hi_mask = 0xFF,
     .mid_reg = 0x71,
     .lo_reg = 0x72,
     .lo_mask = 0xFF,
     .lo_bits = 8},
};

constexpr AxpEvent EVENTS[] = {
    {.name = "power_key_short", .status_index = 0, .bit = 3, .enable_index = 0, .enable_bit = 3},
    {.name = "vbus_insert", .status_index = 1, .bit = 7, .enable_index = 1, .enable_bit = 7},
    {.name = "vbus_remove", .status_index = 1, .bit = 6, .enable_index = 1, .enable_bit = 6},
    {.name = "battery_present", .status_index = 1, .bit = 5, .enable_index = 1, .enable_bit = 5},
    {.name = "charge_done", .status_index = 2, .bit = 4, .enable_index = 2, .enable_bit = 4},
};

constexpr AxpStatusFlag STATUS_FLAGS[] = {
    {"battery_present", {0, 0x20, 0x20}},     {"battery_charging", {0, 0x40, 0x40}},
    {"battery_discharging", {0, 0x40, 0x00}}, {"vbus_present", {0, 0x10, 0x10}},
    {"vbus_good", {0, 0x08, 0x08}},           {"battery_active", {0, 0x04, 0x04}},
    {"over_temperature", {1, 0x80, 0x80}},
};

constexpr AxpLutEntry CHARGE_CURRENT[] = {
    {100, 0}, {125, 1}, {150, 2}, {175, 3}, {200, 4}, {300, 5}, {500, 6}, {700, 7},
};

constexpr AxpLutEntry CHARGE_VOLTAGE[] = {
    {4100, 1},
    {4150, 2},
    {4200, 3},
    {4350, 4},
};

constexpr AxpSelect SELECTS[] = {
    {.name = "charge_current", .field = {.entries = CHARGE_CURRENT, .count = 8, .reg = 0x62, .mask = 0x0F, .shift = 0}},
    {.name = "charge_voltage", .field = {.entries = CHARGE_VOLTAGE, .count = 4, .reg = 0x64, .mask = 0x07, .shift = 0}},
};

constexpr AxpNumber NUMBERS[] = {
    {.name = "battery_low_warning", .offset = 5, .step = 1, .field = {0x1A, 0xF0, 4}, .min_code = 0, .max_code = 15},
    {.name = "input_voltage_limit",
     .offset = 3880,
     .step = 80,
     .field = {0x15, 0x0F, 0},
     .min_code = 0,
     .max_code = 15},
    {.name = "precharge_current", .offset = 0, .step = 25, .field = {0x61, 0x0F, 0}, .min_code = 0, .max_code = 8},
};

constexpr AxpFlag FLAGS[] = {
    {.name = "charging", .field = {0x18, 0x02, 1}},
    {.name = "gauge", .field = {0x18, 0x08, 3}},
};

constexpr AxpModel MODEL = {
    .name = "Test AXP",
    .rails = RAILS,
    .adc_channels = ADC_CHANNELS,
    .events = EVENTS,
    .status_flags = STATUS_FLAGS,
    .selects = SELECTS,
    .numbers = NUMBERS,
    .flags = FLAGS,
    .power_off = {0x10, 0x01, 0},
    .restart = {0x10, 0x02, 1},
    .gauge_reset = {0x17, 0x08, 3},
    .chip_id_reg = 0x03,
    .chip_id = 0x4A,
    .status_base = 0x00,
    .status_count = 2,
    .rail_count = 5,
    .adc_count = 3,
    .event_count = 5,
    .status_flag_count = sizeof(STATUS_FLAGS) / sizeof(STATUS_FLAGS[0]),
    .select_count = sizeof(SELECTS) / sizeof(SELECTS[0]),
    .number_count = sizeof(NUMBERS) / sizeof(NUMBERS[0]),
    .flag_count = sizeof(FLAGS) / sizeof(FLAGS[0]),
    .irq_status_base = 0x48,
    .irq_status_count = 3,
    .irq_enable_base = 0x40,
    .gauge_soc_reg = 0xA4,
};

using Write = std::pair<uint8_t, std::vector<uint8_t>>;

/// Register file behind a fake I2C bus.
class RegisterBus final : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                             size_t read_count) override {
    if (address != ADDRESS || this->failure != i2c::ERROR_OK)
      return this->failure;
    if (write_count == 0)
      return i2c::ERROR_OK;

    const uint8_t reg = write_buffer[0];
    if (read_count > 0) {
      for (size_t i = 0; i < read_count; i++)
        read_buffer[i] = this->read(static_cast<uint8_t>(reg + i));
      return i2c::ERROR_OK;
    }

    std::vector<uint8_t> payload(write_buffer + 1, write_buffer + write_count);
    this->writes.emplace_back(reg, payload);
    for (size_t i = 0; i < payload.size(); i++) {
      const uint8_t target = static_cast<uint8_t>(reg + i);
      if (this->write_one_to_clear.count(target) != 0) {
        this->registers[target] &= static_cast<uint8_t>(~payload[i]);
      } else {
        this->registers[target] = payload[i];
      }
    }
    return i2c::ERROR_OK;
  }

  uint8_t read(uint8_t reg) const {
    auto it = this->registers.find(reg);
    return it == this->registers.end() ? 0 : it->second;
  }
  void poke(uint8_t reg, uint8_t value) { this->registers[reg] = value; }

  size_t write_count(uint8_t reg) const {
    size_t count = 0;
    for (const auto &write : this->writes) {
      if (write.first == reg)
        count++;
    }
    return count;
  }

  std::map<uint8_t, uint8_t> registers;
  std::vector<Write> writes;
  /// Registers that clear a bit when a one is written to it, like the interrupt
  /// status registers do.
  std::set<uint8_t> write_one_to_clear;
  i2c::ErrorCode failure{i2c::ERROR_OK};
};

class TestAxp : public AxpComponent {
 public:
  explicit TestAxp(const AxpModel &model = MODEL) : AxpComponent(model) {}

  bool read_for_test(uint8_t reg) {
    uint8_t value = 0;
    return this->read_reg_(reg, value);
  }
  bool update_for_test(uint8_t reg, uint8_t mask, uint8_t value) { return this->update_reg_(reg, mask, value); }
  void set_irq_pending_for_test(bool pending) { this->irq_pending_ = pending; }

  int init_calls{0};
  bool init_result{true};

 protected:
  bool init_chip_() override {
    this->init_calls++;
    return this->init_result;
  }
};

}  // namespace

class AxpBaseTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->axp_.set_i2c_bus(&this->bus_);
    this->axp_.set_i2c_address(ADDRESS);
    // The three interrupt status registers are the only write-one-to-clear ones.
    this->bus_.write_one_to_clear = {0x48, 0x49, 0x4A};
  }

  RegisterBus bus_;
  TestAxp axp_;
};

TEST_F(AxpBaseTest, SetupProbesChipIdAndRunsInitHook) {
  this->bus_.poke(MODEL.chip_id_reg, MODEL.chip_id);

  this->axp_.setup();

  EXPECT_TRUE(this->axp_.is_chip_ready());
  EXPECT_FALSE(this->axp_.is_failed());
  EXPECT_EQ(this->axp_.init_calls, 1);
}

TEST_F(AxpBaseTest, SetupRejectsWrongChipId) {
  this->bus_.poke(MODEL.chip_id_reg, 0x03);

  this->axp_.setup();

  EXPECT_FALSE(this->axp_.is_chip_ready());
  EXPECT_TRUE(this->axp_.is_failed());
  EXPECT_EQ(this->axp_.init_calls, 0);
}

TEST_F(AxpBaseTest, SetupFailsWhenTheBusIsSilent) {
  this->bus_.failure = i2c::ERROR_NOT_ACKNOWLEDGED;

  this->axp_.setup();

  EXPECT_TRUE(this->axp_.is_failed());
  EXPECT_TRUE(this->axp_.status_has_warning());
}

TEST_F(AxpBaseTest, SetupFailsWhenInitHookFails) {
  this->bus_.poke(MODEL.chip_id_reg, MODEL.chip_id);
  this->axp_.init_result = false;

  this->axp_.setup();

  EXPECT_TRUE(this->axp_.is_failed());
}

TEST_F(AxpBaseTest, FindRailMatchesNames) {
  EXPECT_NE(this->axp_.find_rail("DCDC3"), nullptr);
  EXPECT_EQ(this->axp_.find_rail("DCDC9"), nullptr);
  EXPECT_EQ(this->axp_.find_rail(nullptr), nullptr);
}

TEST_F(AxpBaseTest, EncodesVoltageAcrossEveryWindow) {
  const AxpRail *rail = this->axp_.find_rail("DCDC3");
  ASSERT_NE(rail, nullptr);

  uint8_t code = 0xFF;
  ASSERT_TRUE(AxpComponent::encode_voltage(*rail, 500, code));
  EXPECT_EQ(code, 0);
  ASSERT_TRUE(AxpComponent::encode_voltage(*rail, 1200, code));
  EXPECT_EQ(code, 70);
  ASSERT_TRUE(AxpComponent::encode_voltage(*rail, 1220, code));
  EXPECT_EQ(code, 71);
  ASSERT_TRUE(AxpComponent::encode_voltage(*rail, 1540, code));
  EXPECT_EQ(code, 87);
  ASSERT_TRUE(AxpComponent::encode_voltage(*rail, 1600, code));
  EXPECT_EQ(code, 88);
  ASSERT_TRUE(AxpComponent::encode_voltage(*rail, 3400, code));
  EXPECT_EQ(code, 106);

  // Gaps between windows and values off the step grid must be rejected rather
  // than rounded to a neighbouring voltage.
  EXPECT_FALSE(AxpComponent::encode_voltage(*rail, 1210, code));
  EXPECT_FALSE(AxpComponent::encode_voltage(*rail, 1550, code));
  EXPECT_FALSE(AxpComponent::encode_voltage(*rail, 3500, code));
  EXPECT_FALSE(AxpComponent::encode_voltage(*rail, 400, code));
}

TEST_F(AxpBaseTest, VoltageCodeRoundTripsForEveryRail) {
  for (uint8_t r = 0; r < MODEL.rail_count; r++) {
    const AxpRail &rail = MODEL.rails[r];
    if (!axp_rail_has_voltage(rail))
      continue;

    for (uint16_t code = 0; code <= 0xFF; code++) {
      uint16_t millivolt = 0;
      if (!AxpComponent::decode_voltage(rail, static_cast<uint8_t>(code), millivolt))
        continue;
      uint8_t encoded = 0xFF;
      ASSERT_TRUE(AxpComponent::encode_voltage(rail, millivolt, encoded)) << rail.name << " code " << code;
      EXPECT_EQ(encoded, code) << rail.name << " code " << code;
    }
  }
}

TEST_F(AxpBaseTest, RailEnableSetsAndClearsTheBit) {
  const AxpRail *rail = this->axp_.find_rail("DCDC3");
  ASSERT_NE(rail, nullptr);

  ASSERT_TRUE(this->axp_.set_rail_enabled(rail, true));
  EXPECT_EQ(this->bus_.read(0x12), 0x02);
  EXPECT_TRUE(this->axp_.is_rail_enabled(rail));

  ASSERT_TRUE(this->axp_.set_rail_enabled(rail, false));
  EXPECT_EQ(this->bus_.read(0x12), 0x00);
  EXPECT_FALSE(this->axp_.is_rail_enabled(rail));
}

TEST_F(AxpBaseTest, RailEnableKeepsOtherRailsAlone) {
  this->bus_.poke(0x12, 0x01);
  const AxpRail *rail = this->axp_.find_rail("DCDC3");
  ASSERT_NE(rail, nullptr);

  ASSERT_TRUE(this->axp_.set_rail_enabled(rail, true));

  EXPECT_EQ(this->bus_.read(0x12), 0x03);
}

TEST_F(AxpBaseTest, AlwaysOnRailRefusesToBeDisabled) {
  const AxpRail *rail = this->axp_.find_rail("DCDC1");
  ASSERT_NE(rail, nullptr);
  this->bus_.writes.clear();

  EXPECT_FALSE(this->axp_.set_rail_enabled(rail, false));
  EXPECT_TRUE(this->bus_.writes.empty());
}

TEST_F(AxpBaseTest, RailWithoutSwitchIsAlwaysOn) {
  const AxpRail *rail = this->axp_.find_rail("LDO1");
  ASSERT_NE(rail, nullptr);

  EXPECT_TRUE(this->axp_.is_rail_enabled(rail));
  EXPECT_TRUE(this->axp_.set_rail_enabled(rail, true));
  EXPECT_FALSE(this->axp_.set_rail_enabled(rail, false));
  EXPECT_TRUE(this->bus_.writes.empty());
}

TEST_F(AxpBaseTest, RailWithoutVoltageControlRejectsVoltageCalls) {
  const AxpRail *rail = this->axp_.find_rail("EXTEN");
  ASSERT_NE(rail, nullptr);

  uint16_t millivolt = 0;
  EXPECT_FALSE(this->axp_.set_rail_voltage(rail, 3300));
  EXPECT_FALSE(this->axp_.get_rail_voltage(rail, millivolt));
}

TEST_F(AxpBaseTest, RailVoltageWritesTheRegisterCode) {
  const AxpRail *rail = this->axp_.find_rail("ALDO1");
  ASSERT_NE(rail, nullptr);

  ASSERT_TRUE(this->axp_.set_rail_voltage(rail, 2800));
  EXPECT_EQ(this->bus_.read(0x92), 23);

  uint16_t millivolt = 0;
  ASSERT_TRUE(this->axp_.get_rail_voltage(rail, millivolt));
  EXPECT_EQ(millivolt, 2800);

  // 2850 mV is not on the 100 mV grid, so it must be refused.
  EXPECT_FALSE(this->axp_.set_rail_voltage(rail, 2850));
  EXPECT_EQ(this->bus_.read(0x92), 23);
}

TEST_F(AxpBaseTest, UpdateRegisterSkipsRedundantWrites) {
  this->bus_.poke(0x12, 0x02);
  this->bus_.writes.clear();
  const AxpRail *rail = this->axp_.find_rail("DCDC3");
  ASSERT_NE(rail, nullptr);

  ASSERT_TRUE(this->axp_.set_rail_enabled(rail, true));

  EXPECT_TRUE(this->bus_.writes.empty());
}

TEST_F(AxpBaseTest, AdcDecodesHigh8Low4) {
  this->bus_.poke(0x78, 0x12);
  this->bus_.poke(0x79, 0x30);

  EXPECT_FLOAT_EQ(this->axp_.read_adc(ADC_CHANNELS[0]), 291.0f * 1.1f);
}

TEST_F(AxpBaseTest, AdcDecodesHigh6Low8WithOffset) {
  this->bus_.poke(0x3C, 0x0A);
  this->bus_.poke(0x3D, 0xFF);

  EXPECT_NEAR(this->axp_.read_adc(ADC_CHANNELS[1]), 2815 * 0.1f - 144.7f, 0.01f);
}

TEST_F(AxpBaseTest, AdcDecodesHigh8Mid8Low8) {
  this->bus_.poke(0x70, 0x01);
  this->bus_.poke(0x71, 0x02);
  this->bus_.poke(0x72, 0x03);

  EXPECT_FLOAT_EQ(this->axp_.read_adc(ADC_CHANNELS[2]), 0x010203 * 0.5f);
}

TEST_F(AxpBaseTest, AdcReturnsNanWhenTheBusFails) {
  this->bus_.failure = i2c::ERROR_TIMEOUT;

  EXPECT_TRUE(std::isnan(this->axp_.read_adc(ADC_CHANNELS[0])));
}

TEST_F(AxpBaseTest, AdcEnableTogglesItsChannelBit) {
  ASSERT_TRUE(this->axp_.set_adc_enabled(ADC_CHANNELS[0], true));
  EXPECT_EQ(this->bus_.read(0x82), 0x01);
  ASSERT_TRUE(this->axp_.set_adc_enabled(ADC_CHANNELS[0], false));
  EXPECT_EQ(this->bus_.read(0x82), 0x00);

  // Channels without an enable bit need no bus traffic at all.
  this->bus_.writes.clear();
  EXPECT_TRUE(this->axp_.set_adc_enabled(ADC_CHANNELS[2], true));
  EXPECT_TRUE(this->bus_.writes.empty());
}

TEST_F(AxpBaseTest, StatusBitsComeFromTheCachedRegisters) {
  EXPECT_FALSE(this->axp_.status_bit(STATUS_FLAGS[1].bit)) << "no cache yet";

  this->bus_.poke(0x00, 0x60);
  this->bus_.poke(0x01, 0x80);
  ASSERT_TRUE(this->axp_.update_status());

  EXPECT_TRUE(this->axp_.status_bit(STATUS_FLAGS[1].bit));
  EXPECT_FALSE(this->axp_.status_bit(STATUS_FLAGS[2].bit));
  EXPECT_TRUE(this->axp_.status_bit(STATUS_FLAGS[0].bit));
  EXPECT_FALSE(this->axp_.status_bit(STATUS_FLAGS[3].bit));
  EXPECT_TRUE(this->axp_.status_bit(STATUS_FLAGS[6].bit));
  // A flag the chip does not report is simply absent from its table.
  EXPECT_EQ(this->axp_.find_status_flag("thermal_regulation"), nullptr);

  this->bus_.poke(0x00, 0x20);
  ASSERT_TRUE(this->axp_.update_status());
  EXPECT_FALSE(this->axp_.status_bit(STATUS_FLAGS[1].bit));
  EXPECT_TRUE(this->axp_.status_bit(STATUS_FLAGS[2].bit));
}

TEST_F(AxpBaseTest, StatusFlagsAreFoundByName) {
  const AxpStatusFlag *flag = this->axp_.find_status_flag("vbus_good");
  ASSERT_NE(flag, nullptr);
  EXPECT_EQ(flag->bit.mask, 0x08);
  EXPECT_EQ(this->axp_.find_status_flag("nonsense"), nullptr);
  EXPECT_EQ(this->axp_.find_status_flag(nullptr), nullptr);
}

TEST_F(AxpBaseTest, StatusCacheIsDroppedOnBusFailure) {
  this->bus_.poke(0x00, 0x40);
  ASSERT_TRUE(this->axp_.update_status());
  EXPECT_TRUE(this->axp_.status_bit(STATUS_FLAGS[1].bit));

  this->bus_.failure = i2c::ERROR_NOT_ACKNOWLEDGED;
  EXPECT_FALSE(this->axp_.update_status());
  EXPECT_FALSE(this->axp_.status_bit(STATUS_FLAGS[1].bit));
}

TEST_F(AxpBaseTest, ReadAndClearEventsReportsEveryPendingBit) {
  this->bus_.poke(0x48, 0x08);
  this->bus_.poke(0x49, 0xC0);
  this->bus_.poke(0x4A, 0x00);

  uint64_t mask = 0;
  ASSERT_TRUE(this->axp_.read_and_clear_events(mask));

  EXPECT_EQ(mask, (uint64_t{1} << 0) | (uint64_t{1} << 1) | (uint64_t{1} << 2));
}

TEST_F(AxpBaseTest, ReadAndClearEventsWritesBackExactlyWhatItRead) {
  this->bus_.poke(0x48, 0x08);
  this->bus_.poke(0x49, 0xC0);
  this->bus_.poke(0x4A, 0x00);
  this->bus_.writes.clear();

  uint64_t mask = 0;
  ASSERT_TRUE(this->axp_.read_and_clear_events(mask));

  // Write-one-to-clear: writing back the read bytes clears only the observed
  // bits, so a bit set between the read and the write survives.
  ASSERT_EQ(this->bus_.writes.size(), 1u);
  EXPECT_EQ(this->bus_.writes[0].first, 0x48);
  EXPECT_EQ(this->bus_.writes[0].second, (std::vector<uint8_t>{0x08, 0xC0, 0x00}));
}

TEST_F(AxpBaseTest, ReadAndClearEventsStaysQuietWhenNothingIsPending) {
  uint64_t mask = 0xFFFF;
  ASSERT_TRUE(this->axp_.read_and_clear_events(mask));

  EXPECT_EQ(mask, 0u);
  EXPECT_TRUE(this->bus_.writes.empty());
}

TEST_F(AxpBaseTest, ReadAndClearEventsFailsOnBusError) {
  this->bus_.failure = i2c::ERROR_NOT_ACKNOWLEDGED;

  uint64_t mask = 0xFFFF;
  EXPECT_FALSE(this->axp_.read_and_clear_events(mask));
  EXPECT_EQ(mask, 0u);
}

TEST_F(AxpBaseTest, SetEventsEnabledWritesOneBytePerRegister) {
  ASSERT_TRUE(this->axp_.set_events_enabled((uint64_t{1} << 0) | (uint64_t{1} << 1) | (uint64_t{1} << 4)));

  EXPECT_EQ(this->bus_.read(0x40), 0x08);
  EXPECT_EQ(this->bus_.read(0x41), 0x80);
  EXPECT_EQ(this->bus_.read(0x42), 0x10);
  EXPECT_EQ(this->bus_.writes.size(), 3u);
}

TEST_F(AxpBaseTest, SetEventsEnabledClearsWhatIsNotRequested) {
  this->bus_.poke(0x40, 0xFF);
  this->bus_.poke(0x41, 0xFF);
  this->bus_.poke(0x42, 0xFF);

  ASSERT_TRUE(this->axp_.set_events_enabled(uint64_t{1} << 4));

  EXPECT_EQ(this->bus_.read(0x40), 0x00);
  EXPECT_EQ(this->bus_.read(0x41), 0x00);
  EXPECT_EQ(this->bus_.read(0x42), 0x10);
}

TEST_F(AxpBaseTest, LutSnapsToTheClosestEntry) {
  uint16_t applied = 0;

  ASSERT_TRUE(this->axp_.write_lut_value(MODEL.selects[0].field, 130, &applied));
  EXPECT_EQ(applied, 125);
  EXPECT_EQ(this->bus_.read(0x62), 1);

  ASSERT_TRUE(this->axp_.write_lut_value(MODEL.selects[0].field, 480, &applied));
  EXPECT_EQ(applied, 500);
  EXPECT_EQ(this->bus_.read(0x62), 6);

  // Above the table the largest entry wins instead of overflowing the field.
  ASSERT_TRUE(this->axp_.write_lut_value(MODEL.selects[0].field, 1300, &applied));
  EXPECT_EQ(applied, 700);
  EXPECT_EQ(this->bus_.read(0x62), 7);
}

TEST_F(AxpBaseTest, LutReportsAppliedValueWithoutAnOutParameter) {
  EXPECT_TRUE(this->axp_.write_lut_value(MODEL.selects[0].field, 125));
  EXPECT_EQ(this->bus_.read(0x62), 1);
}

TEST_F(AxpBaseTest, ReadLutValueRejectsUnknownCodes) {
  uint16_t value = 0;

  this->bus_.poke(0x64, 3);
  ASSERT_TRUE(this->axp_.read_lut_value(MODEL.selects[1].field, value));
  EXPECT_EQ(value, 4200);

  this->bus_.poke(0x64, 0);
  EXPECT_FALSE(this->axp_.read_lut_value(MODEL.selects[1].field, value));
}

TEST_F(AxpBaseTest, BusFailureIsReportedOnlyOnce) {
  this->bus_.failure = i2c::ERROR_NOT_ACKNOWLEDGED;

  EXPECT_FALSE(this->axp_.read_for_test(0x12));
  EXPECT_TRUE(this->axp_.status_has_warning());

  // A second failure keeps the warning but must not log again; the status flag
  // is the only observable part, so check it stays set and clears on success.
  EXPECT_FALSE(this->axp_.read_for_test(0x12));
  EXPECT_TRUE(this->axp_.status_has_warning());

  this->bus_.failure = i2c::ERROR_OK;
  this->bus_.poke(0x00, 0x00);
  EXPECT_TRUE(this->axp_.update_status());
  EXPECT_FALSE(this->axp_.status_has_warning());
}

TEST_F(AxpBaseTest, UpdateForwardsTheMaskedWrite) {
  this->bus_.poke(0x62, 0xF0);

  ASSERT_TRUE(this->axp_.update_for_test(0x62, 0x0F, 0x03));

  EXPECT_EQ(this->bus_.read(0x62), 0xF3);
}

TEST_F(AxpBaseTest, ChargingAndGaugeUseTheirOwnBits) {
  this->bus_.poke(0x18, 0x00);

  ASSERT_TRUE(this->axp_.set_charging_enabled(true));
  EXPECT_EQ(this->bus_.read(0x18), 0x02);
  EXPECT_TRUE(this->axp_.is_charging_enabled());

  ASSERT_TRUE(this->axp_.set_gauge_enabled(true));
  EXPECT_EQ(this->bus_.read(0x18), 0x0A);
  EXPECT_TRUE(this->axp_.is_gauge_enabled());

  ASSERT_TRUE(this->axp_.set_charging_enabled(false));
  EXPECT_EQ(this->bus_.read(0x18), 0x08) << "the gauge bit must survive";

  ASSERT_TRUE(this->axp_.set_gauge_enabled(false));
  EXPECT_EQ(this->bus_.read(0x18), 0x00);
}

TEST_F(AxpBaseTest, ChargeCurrentAndVoltageGoThroughTheLookupTables) {
  uint16_t applied = 0;
  ASSERT_TRUE(this->axp_.set_charge_current(200, &applied));
  EXPECT_EQ(applied, 200);
  EXPECT_EQ(this->bus_.read(0x62), 4);

  uint16_t milliamps = 0;
  ASSERT_TRUE(this->axp_.get_charge_current(milliamps));
  EXPECT_EQ(milliamps, 200);

  ASSERT_TRUE(this->axp_.set_charge_voltage(4200, &applied));
  EXPECT_EQ(applied, 4200);
  EXPECT_EQ(this->bus_.read(0x64), 3);

  uint16_t millivolt = 0;
  ASSERT_TRUE(this->axp_.get_charge_voltage(millivolt));
  EXPECT_EQ(millivolt, 4200);
}

TEST_F(AxpBaseTest, StateOfChargeReadsTheGaugeRegister) {
  this->bus_.poke(MODEL.gauge_soc_reg, 73);

  uint8_t percent = 0;
  ASSERT_TRUE(this->axp_.read_state_of_charge(percent));
  EXPECT_EQ(percent, 73);
}

// A chip without a charger, gauge or state of charge register must refuse these
// calls instead of touching the bus.
namespace {

constexpr AxpModel BARE_MODEL = {
    .name = "Bare AXP",
    .gauge_soc_reg = AXP_REG_NONE,
};

class BareAxp : public AxpComponent {
 public:
  BareAxp() : AxpComponent(BARE_MODEL) {}
};

}  // namespace

TEST_F(AxpBaseTest, MissingChargerAndGaugeFeaturesReportFailure) {
  BareAxp bare;
  bare.set_i2c_bus(&this->bus_);
  bare.set_i2c_address(ADDRESS);

  EXPECT_FALSE(bare.set_charging_enabled(true));
  EXPECT_FALSE(bare.is_charging_enabled());
  EXPECT_FALSE(bare.set_gauge_enabled(true));
  EXPECT_FALSE(bare.is_gauge_enabled());
  EXPECT_FALSE(bare.set_charge_current(200));
  EXPECT_FALSE(bare.set_charge_voltage(4200));

  uint8_t percent = 0;
  EXPECT_FALSE(bare.read_state_of_charge(percent));
  EXPECT_TRUE(this->bus_.writes.empty()) << "no bus traffic expected";
}

TEST_F(AxpBaseTest, AdcChannelIsFoundByName) {
  EXPECT_NE(this->axp_.find_adc_channel("battery_voltage"), nullptr);
  EXPECT_EQ(this->axp_.find_adc_channel("battery_current"), nullptr);
  EXPECT_EQ(this->axp_.find_adc_channel(nullptr), nullptr);
}

namespace {

/// Stands in for a platform entity: records how often the hub refreshed it.
class RecordingEntity : public AxpPolledEntity {
 public:
  explicit RecordingEntity(int *publish_count) : publish_count_(publish_count) {}

  void publish_from_chip() override { (*this->publish_count_)++; }

 protected:
  int *publish_count_;
};

}  // namespace

TEST_F(AxpBaseTest, HubRefreshesEveryRegisteredEntityFromOnePoll) {
  int first = 0;
  int second = 0;
  RecordingEntity a(&first);
  RecordingEntity b(&second);
  this->axp_.add_polled_entity(&a);
  this->axp_.add_polled_entity(&b);

  this->axp_.update();

  EXPECT_EQ(first, 1);
  EXPECT_EQ(second, 1);
}

TEST_F(AxpBaseTest, HubRefreshesEntitiesOnlyWhenPolled) {
  int published = 0;
  RecordingEntity entity(&published);
  this->axp_.add_polled_entity(&entity);

  EXPECT_EQ(published, 0);
}

TEST_F(AxpBaseTest, NearestVoltageRoundsToTheClosestReachableValue) {
  const AxpRail *rail = this->axp_.find_rail("DCDC3");
  ASSERT_NE(rail, nullptr);

  uint8_t code = 0xFF;
  uint16_t applied = 0;

  // Values on the grid are taken as they are, whichever window they fall in.
  ASSERT_TRUE(AxpComponent::encode_voltage_nearest(*rail, 3300, code, applied));
  EXPECT_EQ(code, 105);
  EXPECT_EQ(applied, 3300);

  // A value between two steps picks the nearer one rather than the lower one.
  ASSERT_TRUE(AxpComponent::encode_voltage_nearest(*rail, 3349, code, applied));
  EXPECT_EQ(code, 105);
  EXPECT_EQ(applied, 3300);
  ASSERT_TRUE(AxpComponent::encode_voltage_nearest(*rail, 3351, code, applied));
  EXPECT_EQ(code, 106);
  EXPECT_EQ(applied, 3400);

  // Inside the 10 mV window the step is finer, and the midpoint rounds up.
  ASSERT_TRUE(AxpComponent::encode_voltage_nearest(*rail, 1004, code, applied));
  EXPECT_EQ(applied, 1000);
  ASSERT_TRUE(AxpComponent::encode_voltage_nearest(*rail, 1006, code, applied));
  EXPECT_EQ(applied, 1010);

  // Outside the whole range the nearest end is used instead of failing.
  ASSERT_TRUE(AxpComponent::encode_voltage_nearest(*rail, 0, code, applied));
  EXPECT_EQ(code, 0);
  EXPECT_EQ(applied, 500);
  ASSERT_TRUE(AxpComponent::encode_voltage_nearest(*rail, 5000, code, applied));
  EXPECT_EQ(code, 106);
  EXPECT_EQ(applied, 3400);
}

TEST_F(AxpBaseTest, NearestVoltageBridgesTheGapBetweenWindows) {
  const AxpRail *rail = this->axp_.find_rail("DCDC3");
  ASSERT_NE(rail, nullptr);

  // 1210 mV is in the gap between the 10 mV and 20 mV windows; the nearest
  // reachable voltages are 1200 and 1220, and 1220 is closer.
  uint8_t code = 0xFF;
  uint16_t applied = 0;
  ASSERT_TRUE(AxpComponent::encode_voltage_nearest(*rail, 1212, code, applied));
  EXPECT_EQ(code, 71);
  EXPECT_EQ(applied, 1220);
  ASSERT_TRUE(AxpComponent::encode_voltage_nearest(*rail, 1208, code, applied));
  EXPECT_EQ(code, 70);
  EXPECT_EQ(applied, 1200);

  // The same at the next window boundary.
  ASSERT_TRUE(AxpComponent::encode_voltage_nearest(*rail, 1550, code, applied));
  EXPECT_EQ(applied, 1540);
  ASSERT_TRUE(AxpComponent::encode_voltage_nearest(*rail, 1580, code, applied));
  EXPECT_EQ(applied, 1600);
}

TEST_F(AxpBaseTest, NearestVoltageHasNoAnswerWithoutVoltageControl) {
  const AxpRail *rail = this->axp_.find_rail("EXTEN");
  ASSERT_NE(rail, nullptr);

  uint8_t code = 0xFF;
  uint16_t applied = 0;
  EXPECT_FALSE(AxpComponent::encode_voltage_nearest(*rail, 1800, code, applied));
}

TEST_F(AxpBaseTest, CountRailLevelsCountsWhatTheRangeCanReach) {
  const AxpRail &wide = *this->axp_.find_rail("DCDC3");
  const AxpRail &narrow = *this->axp_.find_rail("ALDO1");

  // 500..1200 in 10 mV steps is 71, 1220..1540 in 20 mV is 17 and 1600..3400
  // in 100 mV is 19, so the whole rail has 107 levels.
  EXPECT_EQ(AxpComponent::count_rail_levels(wide, 500, 3400), 107);

  // The 100 mV window alone: 1600..3400 is 19 levels.
  EXPECT_EQ(AxpComponent::count_rail_levels(wide, 1600, 3400), 19);

  // A dimmer's typical 2.8..3.3 V window only reaches 6 of them, which is why
  // the platform warns below four.
  EXPECT_EQ(AxpComponent::count_rail_levels(wide, 2800, 3300), 6);

  // A single-window rail: 500..3500 in 100 mV steps.
  EXPECT_EQ(AxpComponent::count_rail_levels(narrow, 500, 3500), 31);
  EXPECT_EQ(AxpComponent::count_rail_levels(narrow, 2800, 3300), 6);

  // A window with no reachable voltage is empty rather than an error.
  EXPECT_EQ(AxpComponent::count_rail_levels(narrow, 100, 400), 0);
  EXPECT_EQ(AxpComponent::count_rail_levels(narrow, 2000, 1999), 0);
}

TEST_F(AxpBaseTest, DvmIsOnlyAvailableOnTheRailsThatHaveIt) {
  EXPECT_TRUE(this->axp_.set_rail_dvm(this->axp_.find_rail("DCDC3"), true));
  EXPECT_EQ(this->bus_.read(0x27) & 0x80, 0x80);

  // Turning it off clears the bit and leaves the voltage code alone.
  this->bus_.poke(0x27, 0x80 | 88);
  EXPECT_TRUE(this->axp_.set_rail_dvm(this->axp_.find_rail("DCDC3"), false));
  EXPECT_EQ(this->bus_.read(0x27), 88);

  // A write to the voltage keeps the DVM bit, which is what makes it survive
  // the level changes a dimmer makes.
  this->bus_.poke(0x27, 0x80 | 88);
  ASSERT_TRUE(this->axp_.set_rail_voltage(this->axp_.find_rail("DCDC3"), 3300));
  EXPECT_EQ(this->bus_.read(0x27), 0x80 | 105);

  EXPECT_FALSE(this->axp_.set_rail_dvm(this->axp_.find_rail("ALDO1"), true));
  // A rail without voltage control has nowhere to put the bit either.
  EXPECT_FALSE(this->axp_.set_rail_dvm(this->axp_.find_rail("EXTEN"), true));
  EXPECT_FALSE(this->axp_.set_rail_dvm(nullptr, true));
}

TEST_F(AxpBaseTest, SetRailVoltageNearestReportsTheAppliedValue) {
  const AxpRail *rail = this->axp_.find_rail("ALDO1");
  ASSERT_NE(rail, nullptr);

  uint16_t applied = 0;
  ASSERT_TRUE(this->axp_.set_rail_voltage_nearest(rail, 2812, &applied));
  EXPECT_EQ(applied, 2800);
  EXPECT_EQ(this->bus_.read(0x92) & 0x1F, 23);

  // The exact-setter still refuses a value between steps, so callers that must
  // not silently change the voltage keep using it.
  EXPECT_FALSE(this->axp_.set_rail_voltage(rail, 2812));

  EXPECT_FALSE(this->axp_.set_rail_voltage_nearest(nullptr, 2800));
  EXPECT_FALSE(this->axp_.set_rail_voltage_nearest(this->axp_.find_rail("EXTEN"), 2800));
}

TEST_F(AxpBaseTest, EventIndexUsesTheModelOrder) {
  EXPECT_EQ(this->axp_.find_event_index("power_key_short"), 0);
  EXPECT_EQ(this->axp_.find_event_index("vbus_remove"), 2);
  EXPECT_EQ(this->axp_.find_event_index("charge_done"), 4);

  EXPECT_EQ(this->axp_.find_event_index("power_key_long"), -1);
  EXPECT_EQ(this->axp_.find_event_index(nullptr), -1);
}

TEST_F(AxpBaseTest, AddingEventsKeepsTheOnesAlreadyEnabled) {
  // Two entities each watch a different source; neither may switch the other's
  // interrupt off.
  ASSERT_TRUE(this->axp_.add_events_enabled(uint64_t{1} << 1));
  EXPECT_EQ(this->bus_.read(0x41), 0x80);

  ASSERT_TRUE(this->axp_.add_events_enabled(uint64_t{1} << 4));
  EXPECT_EQ(this->bus_.read(0x41), 0x80);
  EXPECT_EQ(this->bus_.read(0x42), 0x10);
}

TEST_F(AxpBaseTest, FirstEventSubscriptionDropsWhatTheChipLatchedBefore) {
  // The chip latches interrupts whether or not anything reads them, so an event
  // from before anyone listened must not be reported as if it just happened.
  this->bus_.poke(0x49, 0x80);

  ASSERT_TRUE(this->axp_.add_events_enabled(uint64_t{1} << 1));

  uint64_t mask = 0;
  ASSERT_TRUE(this->axp_.read_and_clear_events(mask));
  EXPECT_EQ(mask, 0);
}

TEST_F(AxpBaseTest, LaterSubscriptionsLeavePendingEventsAlone) {
  ASSERT_TRUE(this->axp_.add_events_enabled(uint64_t{1} << 1));

  // An event that arrives while the first entity is already listening stays.
  this->bus_.poke(0x49, 0x40);
  ASSERT_TRUE(this->axp_.add_events_enabled(uint64_t{1} << 4));

  uint64_t mask = 0;
  ASSERT_TRUE(this->axp_.read_and_clear_events(mask));
  EXPECT_EQ(mask, uint64_t{1} << 2);  // vbus_remove
}

TEST_F(AxpBaseTest, EventsAreOnlyReadWhenSomethingListens) {
  LoopBlockingGuard guard(nullptr, nullptr, 10000);
  this->bus_.poke(0x49, 0x80);

  this->axp_.loop();

  // Nobody subscribed, so the status registers were left alone.
  EXPECT_EQ(this->bus_.write_count(0x48), 0);
}

TEST_F(AxpBaseTest, PollingWaitsForTheInterval) {
  this->axp_.set_event_poll_interval(100);
  std::vector<uint64_t> seen;
  this->axp_.add_on_event([&seen](uint64_t mask) { seen.push_back(mask); });
  ASSERT_TRUE(this->axp_.add_events_enabled(uint64_t{1} << 1));

  this->bus_.poke(0x49, 0x80);  // vbus_insert
  {
    LoopBlockingGuard guard(nullptr, nullptr, 0);
    this->axp_.loop();
  }
  EXPECT_TRUE(seen.empty());

  {
    LoopBlockingGuard guard(nullptr, nullptr, 100);
    this->axp_.loop();
  }
  ASSERT_EQ(seen.size(), 1u);
  EXPECT_EQ(seen[0], uint64_t{1} << 1);

  // Nothing new to report, so the next pass reads nothing.
  {
    LoopBlockingGuard guard(nullptr, nullptr, 150);
    this->axp_.loop();
  }
  EXPECT_EQ(seen.size(), 1u);

  this->bus_.poke(0x4A, 0x10);  // charge_done
  {
    LoopBlockingGuard guard(nullptr, nullptr, 200);
    this->axp_.loop();
  }
  ASSERT_EQ(seen.size(), 2u);
  EXPECT_EQ(seen[1], uint64_t{1} << 4);
}

TEST_F(AxpBaseTest, PollingCanBeDisabled) {
  this->axp_.set_event_poll_interval(0);
  int seen = 0;
  this->axp_.add_on_event([&seen](uint64_t) { seen++; });
  ASSERT_TRUE(this->axp_.add_events_enabled(uint64_t{1} << 1));

  this->bus_.poke(0x49, 0x80);
  LoopBlockingGuard guard(nullptr, nullptr, 100000);
  this->axp_.loop();

  EXPECT_EQ(seen, 0);
}

TEST_F(AxpBaseTest, WiredInterruptsAreReadWithoutWaitingForTheInterval) {
  // With a pin the chip already told us, so there is no reason to hold the read
  // back until the poll interval is over.
  host::HostGPIOPin pin;
  pin.set_pin(0);
  pin.set_flags(gpio::Flags::FLAG_INPUT);
  this->axp_.set_irq_pin(&pin);
  this->axp_.set_event_poll_interval(10000);

  int seen = 0;
  this->axp_.add_on_event([&seen](uint64_t) { seen++; });
  ASSERT_TRUE(this->axp_.add_events_enabled(uint64_t{1} << 1));

  this->bus_.poke(0x49, 0x80);
  this->axp_.set_irq_pending_for_test(true);
  LoopBlockingGuard guard(nullptr, nullptr, 0);
  this->axp_.loop();

  EXPECT_EQ(seen, 1);

  // A pending flag that is already handled must not cause another read.
  this->axp_.loop();
  EXPECT_EQ(seen, 1);
}

TEST_F(AxpBaseTest, OneShotControlsWriteOnlyTheirOwnBit) {
  // The three control bits share registers with configuration bits, which have
  // to survive the write.
  this->bus_.poke(0x10, 0x30);

  ASSERT_TRUE(this->axp_.power_off());
  EXPECT_EQ(this->bus_.read(0x10), 0x31);

  ASSERT_TRUE(this->axp_.restart());
  EXPECT_EQ(this->bus_.read(0x10), 0x33);

  this->bus_.poke(0x17, 0x04);
  ASSERT_TRUE(this->axp_.reset_gauge());
  EXPECT_EQ(this->bus_.read(0x17), 0x0C);
}

TEST_F(AxpBaseTest, OneShotControlsFailWhenTheChipHasNone) {
  AxpModel model = MODEL;
  model.power_off = {AXP_REG_NONE, 0, 0};

  TestAxp axp(model);
  axp.set_i2c_bus(&this->bus_);
  axp.set_i2c_address(ADDRESS);

  EXPECT_FALSE(axp.power_off());
  // A chip without the control must not write to register 0xFF either.
  EXPECT_TRUE(this->bus_.writes.empty());
}

TEST_F(AxpBaseTest, OneShotControlsFailOnBusError) {
  this->bus_.failure = i2c::ERROR_NOT_ACKNOWLEDGED;

  EXPECT_FALSE(this->axp_.power_off());
  EXPECT_FALSE(this->axp_.restart());
  EXPECT_FALSE(this->axp_.reset_gauge());
}

TEST_F(AxpBaseTest, SelectIsFoundByName) {
  const AxpSelect *current = this->axp_.find_select("charge_current");
  ASSERT_NE(current, nullptr);
  EXPECT_STREQ(current->name, "charge_current");
  EXPECT_EQ(current->field.reg, 0x62);

  EXPECT_EQ(this->axp_.find_select("vbus_current"), nullptr);
  EXPECT_EQ(this->axp_.find_select(nullptr), nullptr);
}

TEST_F(AxpBaseTest, SelectOptionIndexMapsToItsTableEntry) {
  const AxpSelect &select = *this->axp_.find_select("charge_current");

  // Option 3 in the table above is 175 mA, code 3.
  ASSERT_TRUE(this->axp_.write_select_index(select, 3));
  EXPECT_EQ(this->bus_.read(0x62), 0x03);

  uint8_t code = 0xFF;
  ASSERT_TRUE(this->axp_.read_select_code(select, code));
  EXPECT_EQ(code, 0x03);

  // Only steps the table lists are options.
  EXPECT_FALSE(this->axp_.write_select_index(select, 8));
}

TEST_F(AxpBaseTest, AWriteAfterTheTableMovedStillWritesTheRightCode) {
  // The option index is the table index, so a table with gaps in its codes
  // still needs no reverse lookup on the way out.
  const AxpSelect &select = *this->axp_.find_select("charge_voltage");

  ASSERT_TRUE(this->axp_.write_select_index(select, 3));
  EXPECT_EQ(this->bus_.read(0x64), 0x04);  // 4350 mV, code 4
}

TEST_F(AxpBaseTest, NumberIsFoundByName) {
  const AxpNumber *warning = this->axp_.find_number("battery_low_warning");
  ASSERT_NE(warning, nullptr);
  EXPECT_EQ(warning->field.reg, 0x1A);

  EXPECT_EQ(this->axp_.find_number("low_battery"), nullptr);
  EXPECT_EQ(this->axp_.find_number(nullptr), nullptr);
}

TEST_F(AxpBaseTest, NumberCodesDecodeToValues) {
  const AxpNumber &warning = *this->axp_.find_number("battery_low_warning");
  EXPECT_EQ(AxpComponent::decode_number(warning, 0), 5);
  EXPECT_EQ(AxpComponent::decode_number(warning, 15), 20);

  const AxpNumber &limit = *this->axp_.find_number("input_voltage_limit");
  EXPECT_EQ(AxpComponent::decode_number(limit, 0), 3880);
  EXPECT_EQ(AxpComponent::decode_number(limit, 1), 3960);
  EXPECT_EQ(AxpComponent::decode_number(limit, 15), 5080);
}

TEST_F(AxpBaseTest, NumberValuesRoundToTheClosestCode) {
  const AxpNumber &limit = *this->axp_.find_number("input_voltage_limit");

  EXPECT_EQ(AxpComponent::encode_number(limit, 3960), 1);
  // 3920 mV is exactly between 3880 and 3960, and rounds up.
  EXPECT_EQ(AxpComponent::encode_number(limit, 3920), 1);
  EXPECT_EQ(AxpComponent::encode_number(limit, 3919), 0);
  EXPECT_EQ(AxpComponent::encode_number(limit, 3930), 1);

  // Values outside the setting's own range clamp instead of overflowing.
  EXPECT_EQ(AxpComponent::encode_number(limit, 1000), 0);
  EXPECT_EQ(AxpComponent::encode_number(limit, 9000), 15);
}

TEST_F(AxpBaseTest, NumberCodeRoundTripsForEveryCode) {
  for (uint8_t i = 0; i < MODEL.number_count; i++) {
    const AxpNumber &number = MODEL.numbers[i];
    for (uint8_t code = number.min_code; code <= number.max_code; code++) {
      EXPECT_EQ(AxpComponent::encode_number(number, AxpComponent::decode_number(number, code)), code)
          << number.name << " code " << static_cast<int>(code);
    }
  }
}

TEST_F(AxpBaseTest, NumberWritesOnlyItsOwnBits) {
  // The two battery thresholds share REG 1AH, so each write has to leave the
  // other nibble alone.
  this->bus_.poke(0x1A, 0x0F);
  const AxpNumber &warning = *this->axp_.find_number("battery_low_warning");

  ASSERT_TRUE(this->axp_.write_number_code(warning, 15));
  EXPECT_EQ(this->bus_.read(0x1A), 0xFF);

  uint8_t code = 0;
  ASSERT_TRUE(this->axp_.read_number_code(warning, code));
  EXPECT_EQ(code, 15);
}

TEST_F(AxpBaseTest, NumberRejectsCodesOutsideItsRange) {
  // REG 61H only defines codes 0..8; the rest are reserved by the datasheet.
  const AxpNumber &current = *this->axp_.find_number("precharge_current");

  EXPECT_TRUE(this->axp_.write_number_code(current, 8));
  EXPECT_FALSE(this->axp_.write_number_code(current, 9));
}

TEST_F(AxpBaseTest, FlagIsFoundByName) {
  const AxpFlag *charging = this->axp_.find_flag("charging");
  ASSERT_NE(charging, nullptr);
  EXPECT_EQ(charging->field.reg, 0x18);

  EXPECT_EQ(this->axp_.find_flag("battery"), nullptr);
  EXPECT_EQ(this->axp_.find_flag(nullptr), nullptr);
}

TEST_F(AxpBaseTest, FlagReadsAndWritesItsOwnBit) {
  const AxpFlag &gauge = *this->axp_.find_flag("gauge");
  this->bus_.poke(0x18, 0x02);  // charging on, gauge off

  bool on = true;
  ASSERT_TRUE(this->axp_.read_flag(gauge, on));
  EXPECT_FALSE(on);

  ASSERT_TRUE(this->axp_.write_flag(gauge, true));
  EXPECT_EQ(this->bus_.read(0x18), 0x0A);  // charging bit untouched
  ASSERT_TRUE(this->axp_.read_flag(gauge, on));
  EXPECT_TRUE(on);

  ASSERT_TRUE(this->axp_.write_flag(gauge, false));
  EXPECT_EQ(this->bus_.read(0x18), 0x02);
}

TEST_F(AxpBaseTest, FlagReadFailsOnABusError) {
  this->bus_.failure = i2c::ERROR_TIMEOUT;

  bool on = true;
  EXPECT_FALSE(this->axp_.read_flag(*this->axp_.find_flag("charging"), on));
}

}  // namespace esphome::axp_base::testing
