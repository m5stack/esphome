// These tests pin the AXP2101 model table to the datasheet. They check the
// table itself (addresses, bit positions, voltage windows, lookup tables) rather
// than the shared axp_base mechanics, which axp_base's own tests already cover.

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <string>

#include "esphome/components/axp2101/axp2101.h"

#include "axp2101_test.h"

namespace esphome::axp2101::testing {

TEST_F(Axp2101Test, ModelDescribesTheChipIdAndBuses) {
  const axp_base::AxpModel &model = this->axp_.get_model();

  EXPECT_STREQ(model.name, "AXP2101");
  EXPECT_EQ(model.chip_id_reg, 0x03);
  EXPECT_EQ(model.chip_id, 0x4A);
  EXPECT_EQ(model.status_base, 0x00);
  EXPECT_EQ(model.status_count, 2);
  EXPECT_EQ(model.irq_status_base, 0x48);
  EXPECT_EQ(model.irq_enable_base, 0x40);
  EXPECT_EQ(model.irq_status_count, 3);
  EXPECT_EQ(model.gauge_soc_reg, 0xA4);
}

TEST_F(Axp2101Test, SetupDetectsTheChip) {
  this->axp_.setup();

  EXPECT_TRUE(this->axp_.is_chip_ready());
  EXPECT_FALSE(this->axp_.is_failed());
}

TEST_F(Axp2101Test, SetupRejectsAChipThatIsNotAnAxp2101) {
  this->bus_.poke(0x03, 0x03);  // an AXP192 sitting on the same address

  this->axp_.setup();

  EXPECT_TRUE(this->axp_.is_failed());
}

TEST_F(Axp2101Test, EveryRailIsListedWithItsDatasheetRegister) {
  struct Expectation {
    const char *name;
    uint8_t enable_reg;
    uint8_t enable_mask;
    uint8_t vsel_reg;
    uint8_t vsel_mask;
  };
  // 6.13.2.68 (REG 80H), 6.13.2.75/76 (REG 90H/91H), 6.13.2.70-85 (voltage).
  const Expectation expected[] = {
      {"DCDC1", 0x80, 0x01, 0x82, 0x1F}, {"DCDC2", 0x80, 0x02, 0x83, 0x7F}, {"DCDC3", 0x80, 0x04, 0x84, 0x7F},
      {"DCDC4", 0x80, 0x08, 0x85, 0x7F}, {"DCDC5", 0x80, 0x10, 0x86, 0x1F}, {"ALDO1", 0x90, 0x01, 0x92, 0x1F},
      {"ALDO2", 0x90, 0x02, 0x93, 0x1F}, {"ALDO3", 0x90, 0x04, 0x94, 0x1F}, {"ALDO4", 0x90, 0x08, 0x95, 0x1F},
      {"BLDO1", 0x90, 0x10, 0x96, 0x1F}, {"BLDO2", 0x90, 0x20, 0x97, 0x1F}, {"CPUSLDO", 0x90, 0x40, 0x98, 0x1F},
      {"DLDO1", 0x90, 0x80, 0x99, 0x1F}, {"DLDO2", 0x91, 0x01, 0x9A, 0x1F},
  };

  ASSERT_EQ(this->axp_.get_model().rail_count, sizeof(expected) / sizeof(expected[0]));
  for (const Expectation &e : expected) {
    const axp_base::AxpRail *r = this->rail(e.name);
    ASSERT_NE(r, nullptr) << e.name;
    EXPECT_EQ(r->enable_reg, e.enable_reg) << e.name;
    EXPECT_EQ(r->enable_mask, e.enable_mask) << e.name;
    EXPECT_EQ(r->enable_value, e.enable_mask) << e.name << " (enable is set by writing ones)";
    EXPECT_EQ(r->vsel_reg, e.vsel_reg) << e.name;
    EXPECT_EQ(r->vsel_mask, e.vsel_mask) << e.name;
    EXPECT_EQ(r->vsel_shift, 0) << e.name;
  }
}

TEST_F(Axp2101Test, RailVoltageWindowsTouchWithoutOverlapping) {
  for (uint8_t i = 0; i < this->axp_.get_model().rail_count; i++) {
    const axp_base::AxpRail &r = this->axp_.get_model().rails[i];
    ASSERT_GT(r.range_count, 0) << r.name;
    for (uint8_t k = 0; k < r.range_count; k++) {
      const axp_base::AxpVoltageRange &w = r.ranges[k];
      EXPECT_LT(w.min_code, w.max_code) << r.name << " window " << static_cast<int>(k);
      EXPECT_GT(w.step_mv, 0) << r.name << " window " << static_cast<int>(k);
      if (k + 1 < r.range_count) {
        // The next window must start exactly one code after this one ends.
        EXPECT_EQ(r.ranges[k + 1].min_code, w.max_code + 1) << r.name << " window " << static_cast<int>(k);
      }
    }
    // Every reserved code above the last window must be rejected by the decoder.
    for (uint16_t code = 0; code <= 0xFF; code++) {
      uint16_t millivolt = 0;
      const bool decoded = axp_base::AxpComponent::decode_voltage(r, static_cast<uint8_t>(code), millivolt);
      EXPECT_EQ(decoded, code <= r.ranges[r.range_count - 1].max_code) << r.name << " code " << static_cast<int>(code);
    }
  }
}

TEST_F(Axp2101Test, Dcdc1SpansOnePointFiveToThreePointFourVolts) {
  const axp_base::AxpRail *r = this->rail("DCDC1");
  ASSERT_NE(r, nullptr);
  uint8_t code = 0xFF;

  ASSERT_TRUE(axp_base::AxpComponent::encode_voltage(*r, 1500, code));
  EXPECT_EQ(code, 0);
  ASSERT_TRUE(axp_base::AxpComponent::encode_voltage(*r, 3300, code));
  EXPECT_EQ(code, 18);
  ASSERT_TRUE(axp_base::AxpComponent::encode_voltage(*r, 3400, code));
  EXPECT_EQ(code, 19);

  EXPECT_FALSE(axp_base::AxpComponent::encode_voltage(*r, 1400, code));
  EXPECT_FALSE(axp_base::AxpComponent::encode_voltage(*r, 3500, code));
  EXPECT_FALSE(axp_base::AxpComponent::encode_voltage(*r, 1550, code));
}

TEST_F(Axp2101Test, DcdcVoltageStepsChangeAtTheDocumentedCodes) {
  const axp_base::AxpRail *dcdc2 = this->rail("DCDC2");
  ASSERT_NE(dcdc2, nullptr);
  uint16_t millivolt = 0;

  // 6.13.2.71: 71 codes of 10 mV, then 17 codes of 20 mV.
  ASSERT_TRUE(axp_base::AxpComponent::decode_voltage(*dcdc2, 0, millivolt));
  EXPECT_EQ(millivolt, 500);
  ASSERT_TRUE(axp_base::AxpComponent::decode_voltage(*dcdc2, 70, millivolt));
  EXPECT_EQ(millivolt, 1200);
  ASSERT_TRUE(axp_base::AxpComponent::decode_voltage(*dcdc2, 71, millivolt));
  EXPECT_EQ(millivolt, 1220);
  ASSERT_TRUE(axp_base::AxpComponent::decode_voltage(*dcdc2, 87, millivolt));
  EXPECT_EQ(millivolt, 1540);
  EXPECT_FALSE(axp_base::AxpComponent::decode_voltage(*dcdc2, 88, millivolt));

  // DCDC3 continues past 1.54 V with a 100 mV window up to 3.4 V.
  const axp_base::AxpRail *dcdc3 = this->rail("DCDC3");
  ASSERT_NE(dcdc3, nullptr);
  ASSERT_TRUE(axp_base::AxpComponent::decode_voltage(*dcdc3, 88, millivolt));
  EXPECT_EQ(millivolt, 1600);
  ASSERT_TRUE(axp_base::AxpComponent::decode_voltage(*dcdc3, 106, millivolt));
  EXPECT_EQ(millivolt, 3400);
  EXPECT_FALSE(axp_base::AxpComponent::decode_voltage(*dcdc3, 107, millivolt));

  // DCDC4 stops its second window at 1.84 V.
  const axp_base::AxpRail *dcdc4 = this->rail("DCDC4");
  ASSERT_NE(dcdc4, nullptr);
  ASSERT_TRUE(axp_base::AxpComponent::decode_voltage(*dcdc4, 102, millivolt));
  EXPECT_EQ(millivolt, 1840);
  EXPECT_FALSE(axp_base::AxpComponent::decode_voltage(*dcdc4, 103, millivolt));
}

TEST_F(Axp2101Test, Dcdc5AndLdosUseTheirDocumentedRanges) {
  struct Expectation {
    const char *name;
    uint16_t min_mv;
    uint16_t max_mv;
    uint16_t step_mv;
    uint8_t max_code;
  };
  // Table 6-1 plus 6.13.2.74 and 6.13.2.77 to 6.13.2.85.
  const Expectation expected[] = {
      {"DCDC5", 1400, 3700, 100, 23}, {"ALDO1", 500, 3500, 100, 30},  {"ALDO2", 500, 3500, 100, 30},
      {"ALDO3", 500, 3500, 100, 30},  {"ALDO4", 500, 3500, 100, 30},  {"BLDO1", 500, 3500, 100, 30},
      {"BLDO2", 500, 3500, 100, 30},  {"CPUSLDO", 500, 1400, 50, 18}, {"DLDO1", 500, 3300, 100, 28},
      {"DLDO2", 500, 1400, 50, 18},
  };

  for (const Expectation &e : expected) {
    const axp_base::AxpRail *r = this->rail(e.name);
    ASSERT_NE(r, nullptr) << e.name;
    ASSERT_EQ(r->range_count, 1) << e.name;

    uint8_t code = 0xFF;
    ASSERT_TRUE(axp_base::AxpComponent::encode_voltage(*r, e.min_mv, code)) << e.name;
    EXPECT_EQ(code, 0) << e.name;
    ASSERT_TRUE(axp_base::AxpComponent::encode_voltage(*r, e.max_mv, code)) << e.name;
    EXPECT_EQ(code, e.max_code) << e.name;
    // One step beyond the top must be refused, not clamped.
    EXPECT_FALSE(axp_base::AxpComponent::encode_voltage(*r, e.max_mv + e.step_mv, code)) << e.name;
    EXPECT_FALSE(axp_base::AxpComponent::encode_voltage(*r, e.min_mv - e.step_mv, code)) << e.name;
  }
}

TEST_F(Axp2101Test, RailEnableUsesTheSharedEnableRegisters) {
  const axp_base::AxpRail *aldo4 = this->rail("ALDO4");
  const axp_base::AxpRail *dldo2 = this->rail("DLDO2");
  ASSERT_NE(aldo4, nullptr);
  ASSERT_NE(dldo2, nullptr);

  ASSERT_TRUE(this->axp_.set_rail_enabled(aldo4, true));
  EXPECT_EQ(this->bus_.read(0x90), 0x08);

  // DLDO2 is the only output in the second LDO register.
  ASSERT_TRUE(this->axp_.set_rail_enabled(dldo2, true));
  EXPECT_EQ(this->bus_.read(0x91), 0x01);
  EXPECT_EQ(this->bus_.read(0x90), 0x08) << "the first LDO register must be untouched";
}

TEST_F(Axp2101Test, VoltageWritePreservesTheDvmBit) {
  const axp_base::AxpRail *dcdc3 = this->rail("DCDC3");
  ASSERT_NE(dcdc3, nullptr);
  this->bus_.poke(0x84, 0x80);  // DVM enable lives in bit 7 of the voltage register

  ASSERT_TRUE(this->axp_.set_rail_voltage(dcdc3, 3300));

  EXPECT_EQ(this->bus_.read(0x84), 0x80 | 105);
}

TEST_F(Axp2101Test, AdcChannelsMatchTheDatasheetAddressesAndScaling) {
  struct Expectation {
    const char *name;
    uint8_t enable_bit;
    uint8_t hi_reg;
    uint8_t lo_reg;
    float lsb;
  };
  // 6.13.2.29 and 6.13.2.30 to 6.13.2.39, scaling from Table 6-5. The table
  // stores volts per count so the entities can publish volts directly.
  const Expectation expected[] = {
      {"battery_voltage", 0, 0x34, 0x35, 0.001f},
      {"ts_voltage", 1, 0x36, 0x37, 0.0005f},
      {"vbus_voltage", 2, 0x38, 0x39, 0.001f},
      {"system_voltage", 3, 0x3A, 0x3B, 0.001f},
  };

  ASSERT_EQ(this->axp_.get_model().adc_count, 5);
  for (const Expectation &e : expected) {
    const axp_base::AxpAdcChannel *c = this->channel(e.name);
    ASSERT_NE(c, nullptr) << e.name;
    EXPECT_EQ(c->enable_reg, 0x30) << e.name;
    EXPECT_EQ(c->enable_bit, e.enable_bit) << e.name;
    EXPECT_EQ(c->hi_reg, e.hi_reg) << e.name;
    EXPECT_EQ(c->hi_mask, 0x3F) << e.name;
    EXPECT_EQ(c->mid_reg, axp_base::AXP_REG_NONE) << e.name;
    EXPECT_EQ(c->lo_reg, e.lo_reg) << e.name;
    EXPECT_EQ(c->lo_bits, 8) << e.name;
    EXPECT_FLOAT_EQ(c->lsb, e.lsb) << e.name;
  }
}

TEST_F(Axp2101Test, AdcReadsFourteenBitValues) {
  // 0x2A5A = 10842 counts, 1 mV per count -> 10.842 V.
  this->bus_.poke(0x34, 0x2A);
  this->bus_.poke(0x35, 0x5A);

  const axp_base::AxpAdcChannel *c = this->channel("battery_voltage");
  ASSERT_NE(c, nullptr);
  EXPECT_NEAR(this->axp_.read_adc(*c), 10.842f, 0.0001f);
}

TEST_F(Axp2101Test, AdcIgnoresTheDebugBitsInTheHighRegister) {
  // REG 34H bits 7:6 are channel debug selectors, not part of the value.
  this->bus_.poke(0x34, 0xC1);
  this->bus_.poke(0x35, 0x00);

  const axp_base::AxpAdcChannel *c = this->channel("battery_voltage");
  ASSERT_NE(c, nullptr);
  EXPECT_NEAR(this->axp_.read_adc(*c), 0.256f, 0.0001f);
}

TEST_F(Axp2101Test, DieTemperatureUsesTheDocumentedTransferFunction) {
  const axp_base::AxpAdcChannel *c = this->channel("die_temperature");
  ASSERT_NE(c, nullptr);

  // raw 7274 is the 22 C reference point of the published conversion.
  this->bus_.poke(0x3C, (7274 >> 8) & 0x3F);
  this->bus_.poke(0x3D, 7274 & 0xFF);
  EXPECT_NEAR(this->axp_.read_adc(*c), 22.0f, 0.01f);

  // raw 4814 corresponds to the 145 C thermal shutdown point.
  this->bus_.poke(0x3C, (4814 >> 8) & 0x3F);
  this->bus_.poke(0x3D, 4814 & 0xFF);
  EXPECT_NEAR(this->axp_.read_adc(*c), 145.0f, 0.01f);
}

TEST_F(Axp2101Test, StatusFlagsUseTheDocumentedBits) {
  auto flag = [this](const char *name) {
    const axp_base::AxpStatusFlag *f = this->axp_.find_status_flag(name);
    EXPECT_NE(f, nullptr) << name;
    return f != nullptr ? f->bit : axp_base::AxpStatusBit{};
  };

  // REG 00H: bit 3 battery present, bit 5 VBUS good.
  // REG 01H: bits 6:5 current direction, 01 charge and 10 discharge.
  this->bus_.poke(0x00, 0x28);
  this->bus_.poke(0x01, 0x20);
  ASSERT_TRUE(this->axp_.update_status());

  EXPECT_TRUE(this->axp_.status_bit(flag("battery_present")));
  EXPECT_TRUE(this->axp_.status_bit(flag("vbus_good")));
  EXPECT_TRUE(this->axp_.status_bit(flag("battery_charging")));
  EXPECT_FALSE(this->axp_.status_bit(flag("battery_discharging")));

  this->bus_.poke(0x00, 0x05);  // active mode + current limit, no battery
  this->bus_.poke(0x01, 0x40);
  ASSERT_TRUE(this->axp_.update_status());

  EXPECT_FALSE(this->axp_.status_bit(flag("battery_present")));
  EXPECT_FALSE(this->axp_.status_bit(flag("vbus_good")));
  EXPECT_FALSE(this->axp_.status_bit(flag("battery_charging")));
  EXPECT_TRUE(this->axp_.status_bit(flag("battery_discharging")));
  EXPECT_TRUE(this->axp_.status_bit(flag("battery_active_mode")));
  EXPECT_TRUE(this->axp_.status_bit(flag("current_limit")));
}

TEST_F(Axp2101Test, ChargeCurrentLookupMatchesTheDatasheetTable) {
  struct Pair {
    uint16_t milliamps;
    uint8_t code;
  };
  // 6.13.2.60. Codes 0-3 and 17-31 are reserved or mean "no charging".
  const Pair pairs[] = {{100, 4},  {125, 5},  {150, 6},  {175, 7},  {200, 8},  {300, 9},  {400, 10},
                        {500, 11}, {600, 12}, {700, 13}, {800, 14}, {900, 15}, {1000, 16}};

  for (const Pair &pair : pairs) {
    uint16_t applied = 0;
    ASSERT_TRUE(this->axp_.set_charge_current(pair.milliamps, &applied)) << pair.milliamps;
    EXPECT_EQ(applied, pair.milliamps);
    EXPECT_EQ(this->bus_.read(0x62), pair.code) << pair.milliamps;

    uint16_t read_back = 0;
    ASSERT_TRUE(this->axp_.get_charge_current(read_back)) << pair.milliamps;
    EXPECT_EQ(read_back, pair.milliamps);
  }
}

TEST_F(Axp2101Test, ChargeCurrentSnapsWithinTheTableAndNotToZero) {
  uint16_t applied = 0;

  // Nearest entry wins for values between two codes.
  ASSERT_TRUE(this->axp_.set_charge_current(130, &applied));
  EXPECT_EQ(applied, 125);
  EXPECT_EQ(this->bus_.read(0x62), 5);

  // A request below the smallest entry stays at 100 mA rather than turning
  // charging off, which is what the register's 0 mA code would mean.
  ASSERT_TRUE(this->axp_.set_charge_current(10, &applied));
  EXPECT_EQ(applied, 100);
  EXPECT_EQ(this->bus_.read(0x62), 4);

  // Above the largest entry the 1000 mA code is used instead of overflowing
  // into the reserved codes.
  ASSERT_TRUE(this->axp_.set_charge_current(3000, &applied));
  EXPECT_EQ(applied, 1000);
  EXPECT_EQ(this->bus_.read(0x62), 16);
}

TEST_F(Axp2101Test, ChargeCurrentWriteKeepsTheReservedBits) {
  this->bus_.poke(0x62, 0xE0);  // bits 7:5 are read-only

  ASSERT_TRUE(this->axp_.set_charge_current(200));

  EXPECT_EQ(this->bus_.read(0x62), 0xE8);
}

TEST_F(Axp2101Test, ChargeVoltageLookupMatchesTheDatasheetTable) {
  struct Pair {
    uint16_t millivolt;
    uint8_t code;
  };
  // 6.13.2.62: 001 4.0 V, 010 4.1 V, 011 4.2 V, 100 4.35 V, 101 4.4 V.
  const Pair pairs[] = {{4000, 1}, {4100, 2}, {4200, 3}, {4350, 4}, {4400, 5}};

  for (const Pair &pair : pairs) {
    uint16_t applied = 0;
    ASSERT_TRUE(this->axp_.set_charge_voltage(pair.millivolt, &applied)) << pair.millivolt;
    EXPECT_EQ(applied, pair.millivolt);
    EXPECT_EQ(this->bus_.read(0x64), pair.code) << pair.millivolt;

    uint16_t read_back = 0;
    ASSERT_TRUE(this->axp_.get_charge_voltage(read_back)) << pair.millivolt;
    EXPECT_EQ(read_back, pair.millivolt);
  }

  // Code 0 and the 11x codes are reserved, so reading them must fail.
  uint16_t value = 0;
  this->bus_.poke(0x64, 0x00);
  EXPECT_FALSE(this->axp_.get_charge_voltage(value));
  this->bus_.poke(0x64, 0x06);
  EXPECT_FALSE(this->axp_.get_charge_voltage(value));
}

TEST_F(Axp2101Test, ChargerAndGaugeShareRegister18WithIndependentBits) {
  // 6.13.2.14: bit 1 cell charging, bit 3 gauge, bit 2 button battery, bit 0 watchdog.
  this->bus_.poke(0x18, 0x07);  // button battery + watchdog + charging already on

  ASSERT_TRUE(this->axp_.set_charging_enabled(false));
  EXPECT_EQ(this->bus_.read(0x18), 0x05) << "button battery and watchdog bits must survive";

  ASSERT_TRUE(this->axp_.set_gauge_enabled(true));
  EXPECT_EQ(this->bus_.read(0x18), 0x0D);
  ASSERT_TRUE(this->axp_.set_charging_enabled(true));
  EXPECT_EQ(this->bus_.read(0x18), 0x0F);

  EXPECT_TRUE(this->axp_.is_charging_enabled());
  EXPECT_TRUE(this->axp_.is_gauge_enabled());
}

TEST_F(Axp2101Test, StateOfChargeComesFromRegisterA4) {
  for (uint8_t percent_in : {0, 50, 100}) {
    this->bus_.poke(0xA4, percent_in);
    uint8_t percent = 0xFF;
    ASSERT_TRUE(this->axp_.read_state_of_charge(percent));
    EXPECT_EQ(percent, percent_in);
  }

  // Above 100 the gauge is not reporting a usable value.
  this->bus_.poke(0xA4, 0xFF);
  uint8_t percent = 0;
  EXPECT_FALSE(this->axp_.read_state_of_charge(percent));
}

TEST_F(Axp2101Test, EveryEventPointsAtAValidBit) {
  const axp_base::AxpModel &model = this->axp_.get_model();
  ASSERT_EQ(model.event_count, 24);
  for (uint8_t i = 0; i < model.event_count; i++) {
    const axp_base::AxpEvent &event = model.events[i];
    EXPECT_LT(event.status_index, model.irq_status_count) << event.name;
    EXPECT_LT(event.enable_index, model.irq_status_count) << event.name;
    EXPECT_LT(event.bit, 8) << event.name;
    EXPECT_LT(event.enable_bit, 8) << event.name;
  }
}

TEST_F(Axp2101Test, EventBitsMatchTheDatasheetStatusRegisters) {
  struct Expectation {
    const char *name;
    uint8_t status_index;
    uint8_t bit;
  };
  // 6.13.2.43 to 6.13.2.45.
  const Expectation expected[] = {
      {"soc_warning_level", 0, 7},
      {"soc_shutdown_level", 0, 6},
      {"gauge_watchdog_timeout", 0, 5},
      {"gauge_new_soc", 0, 4},
      {"battery_over_temperature_charging", 0, 3},
      {"battery_under_temperature_charging", 0, 2},
      {"battery_over_temperature_working", 0, 1},
      {"battery_under_temperature_working", 0, 0},
      {"vbus_insert", 1, 7},
      {"vbus_remove", 1, 6},
      {"battery_insert", 1, 5},
      {"battery_remove", 1, 4},
      {"power_key_short_press", 1, 3},
      {"power_key_long_press", 1, 2},
      {"power_key_negative_edge", 1, 1},
      {"power_key_positive_edge", 1, 0},
      {"watchdog_expired", 2, 7},
      {"ldo_over_current", 2, 6},
      {"batfet_over_current", 2, 5},
      {"charge_done", 2, 4},
      {"charge_started", 2, 3},
      {"die_over_temperature", 2, 2},
      {"charger_safety_timer", 2, 1},
      {"battery_over_voltage", 2, 0},
  };

  const axp_base::AxpModel &model = this->axp_.get_model();
  ASSERT_EQ(model.event_count, sizeof(expected) / sizeof(expected[0]));
  for (const Expectation &e : expected) {
    bool found = false;
    for (uint8_t i = 0; i < model.event_count; i++) {
      if (std::string(model.events[i].name) != e.name)
        continue;
      found = true;
      EXPECT_EQ(model.events[i].status_index, e.status_index) << e.name;
      EXPECT_EQ(model.events[i].bit, e.bit) << e.name;
      // Enable and status registers are indexed the same way on this chip.
      EXPECT_EQ(model.events[i].enable_index, e.status_index) << e.name;
      EXPECT_EQ(model.events[i].enable_bit, e.bit) << e.name;
    }
    EXPECT_TRUE(found) << e.name;
  }
}

TEST_F(Axp2101Test, EventsAreReadAndClearedThroughRegisters48To4A) {
  this->bus_.poke(0x49, 0x80);  // VBUS insert
  this->bus_.poke(0x4A, 0x10);  // charge done

  uint64_t mask = 0;
  ASSERT_TRUE(this->axp_.read_and_clear_events(mask));

  // vbus_insert is entry 8, charge_done entry 19 in the table above.
  EXPECT_EQ(mask, (uint64_t{1} << 8) | (uint64_t{1} << 19));
  EXPECT_EQ(this->bus_.read(0x48), 0x00);
}

TEST_F(Axp2101Test, EnablingEventsWritesRegisters40To42) {
  // Enable only the power key short press event (entry 12).
  ASSERT_TRUE(this->axp_.set_events_enabled(uint64_t{1} << 12));

  EXPECT_EQ(this->bus_.read(0x40), 0x00);
  EXPECT_EQ(this->bus_.read(0x41), 0x08);
  EXPECT_EQ(this->bus_.read(0x42), 0x00);
}

TEST_F(Axp2101Test, PowerOffAndRestartWriteReg10) {
  // REG 10H bit 0 is soft PWROFF and bit 1 restarts the SoC (6.13.2.7); the
  // off-discharge and PWRON settings in the same register must survive.
  this->bus_.poke(0x10, 0x30);

  ASSERT_TRUE(this->axp_.power_off());
  EXPECT_EQ(this->bus_.read(0x10), 0x31);

  // The chip clears the action bit again by itself, which the register file
  // here does not model, so start from the same state for the second write.
  this->bus_.poke(0x10, 0x30);
  ASSERT_TRUE(this->axp_.restart());
  EXPECT_EQ(this->bus_.read(0x10), 0x32);
}

TEST_F(Axp2101Test, ResetGaugeWritesReg17) {
  // REG 17H bit 3 resets the fuel gauge (6.13.2.13).
  this->bus_.poke(0x17, 0x04);

  ASSERT_TRUE(this->axp_.reset_gauge());
  EXPECT_EQ(this->bus_.read(0x17), 0x0C);
}

TEST_F(Axp2101Test, InputCurrentLimitWritesReg16) {
  // REG 16H bits 2:0 hold the VBUS input current limit (6.13.2.12), whose value
  // table runs 100 mA to 2000 mA.
  this->bus_.poke(0x16, 0xF8);

  const axp_base::AxpSelect *limit = this->axp_.find_select("input_current_limit");
  ASSERT_NE(limit, nullptr);
  ASSERT_EQ(limit->field.count, 6);

  // Option 4 is 1500 mA, code 4.
  ASSERT_TRUE(this->axp_.write_select_index(*limit, 4));
  EXPECT_EQ(this->bus_.read(0x16), 0xFC);

  uint8_t code = 0xFF;
  ASSERT_TRUE(this->axp_.read_select_code(*limit, code));
  EXPECT_EQ(code, 4);
}

}  // namespace esphome::axp2101::testing
