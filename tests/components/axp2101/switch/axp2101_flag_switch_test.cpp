// Tests for the AXP2101 on/off setting switches (charging, fuel gauge).

#include <gtest/gtest.h>

#include "esphome/components/axp2101/switch/axp2101_flag_switch.h"

#include "../axp2101_test.h"

namespace esphome::axp2101::testing {

namespace {

constexpr uint8_t CONTROL_REG = 0x18;
constexpr uint8_t CHARGING_BIT = 0x02;
constexpr uint8_t GAUGE_BIT = 0x08;

void configure(AXP2101FlagSwitch &sw, axp_base::AxpComponent *parent, const char *name) {
  sw.set_parent(parent);
  sw.set_flag_name(name);
}

}  // namespace

TEST_F(Axp2101Test, FlagSwitchReportsTheChipStateWithoutWriting) {
  // The chip decides at reset, so a switch that was never asked to restore must
  // not change anything: charging is on here by the chip's own configuration.
  this->bus_.poke(CONTROL_REG, CHARGING_BIT | GAUGE_BIT);

  AXP2101FlagSwitch sw;
  configure(sw, &this->axp_, "charging");
  sw.set_restore_mode(switch_::SWITCH_RESTORE_DISABLED);
  sw.setup();
  ASSERT_FALSE(sw.is_failed());

  EXPECT_TRUE(sw.state);
  EXPECT_EQ(this->bus_.writes, 0u) << "setup must not write when nothing asked it to";
}

TEST_F(Axp2101Test, FlagSwitchWritesOnlyItsOwnBit) {
  this->bus_.poke(CONTROL_REG, GAUGE_BIT);

  AXP2101FlagSwitch sw;
  configure(sw, &this->axp_, "charging");
  sw.set_restore_mode(switch_::SWITCH_RESTORE_DISABLED);
  sw.setup();
  ASSERT_FALSE(sw.is_failed());
  ASSERT_FALSE(sw.state);

  sw.turn_on();
  EXPECT_EQ(this->bus_.read(CONTROL_REG), CHARGING_BIT | GAUGE_BIT);
  EXPECT_TRUE(sw.state);

  sw.turn_off();
  EXPECT_EQ(this->bus_.read(CONTROL_REG), GAUGE_BIT);
  EXPECT_FALSE(sw.state);
}

TEST_F(Axp2101Test, FlagSwitchAppliesAConfiguredRestoreMode) {
  // RESTORE_DEFAULT_ON is an explicit request to switch the setting on at boot,
  // unlike the default which leaves the chip alone.
  AXP2101FlagSwitch sw;
  configure(sw, &this->axp_, "gauge");
  sw.set_restore_mode(switch_::SWITCH_RESTORE_DEFAULT_ON);
  sw.setup();
  ASSERT_FALSE(sw.is_failed());

  EXPECT_EQ(this->bus_.read(CONTROL_REG) & GAUGE_BIT, GAUGE_BIT);
  EXPECT_TRUE(sw.state);
}

TEST_F(Axp2101Test, FlagSwitchFailsOnASettingThatIsNotSwitchable) {
  AXP2101FlagSwitch sw;
  configure(sw, &this->axp_, "charge_voltage");  // a value, not an on/off setting
  sw.set_restore_mode(switch_::SWITCH_RESTORE_DISABLED);
  sw.setup();

  EXPECT_TRUE(sw.is_failed());
}

TEST_F(Axp2101Test, FlagSwitchKeepsItsStateWhenTheWriteFails) {
  this->bus_.poke(CONTROL_REG, CHARGING_BIT);

  AXP2101FlagSwitch sw;
  configure(sw, &this->axp_, "charging");
  sw.set_restore_mode(switch_::SWITCH_RESTORE_DISABLED);
  sw.setup();
  ASSERT_TRUE(sw.state);

  this->bus_.failure = i2c::ERROR_TIMEOUT;
  sw.turn_off();

  EXPECT_TRUE(sw.state);
}

}  // namespace esphome::axp2101::testing
