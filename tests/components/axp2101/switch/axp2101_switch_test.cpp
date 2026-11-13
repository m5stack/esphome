// Tests for the AXP2101 rail switches. Living in this subdirectory also tells
// the unit test harness to compile the `switch` platform of this component.

#include <gtest/gtest.h>

#include "esphome/components/axp2101/switch/axp2101_switch.h"

#include "../axp2101_test.h"

namespace esphome::axp2101::testing {

namespace {

constexpr uint8_t ALDO_ENABLE_REG = 0x90;

void configure(AXP2101Switch &sw, axp_base::AxpComponent *parent, const char *rail) {
  sw.set_parent(parent);
  sw.set_rail_name(rail);
  sw.set_restore_mode(switch_::SWITCH_ALWAYS_OFF);
}

}  // namespace

TEST_F(Axp2101Test, SwitchDrivesItsRailsEnableBit) {
  AXP2101Switch sw;
  configure(sw, &this->axp_, "ALDO3");
  sw.setup();
  ASSERT_FALSE(sw.is_failed());

  // ALWAYS_OFF applies the off state during setup.
  EXPECT_EQ(this->bus_.read(ALDO_ENABLE_REG) & 0x04, 0);

  sw.turn_on();
  EXPECT_EQ(this->bus_.read(ALDO_ENABLE_REG) & 0x04, 0x04);
  EXPECT_TRUE(sw.state);

  sw.turn_off();
  EXPECT_EQ(this->bus_.read(ALDO_ENABLE_REG) & 0x04, 0);
  EXPECT_FALSE(sw.state);
}

TEST_F(Axp2101Test, SwitchOnlyTouchesItsOwnBit) {
  // Every LDO shares an enable register, so leaving the other bits alone is the
  // whole job here.
  this->bus_.poke(ALDO_ENABLE_REG, 0xF3);

  AXP2101Switch sw;
  configure(sw, &this->axp_, "ALDO3");
  sw.setup();
  ASSERT_FALSE(sw.is_failed());

  sw.turn_on();
  EXPECT_EQ(this->bus_.read(ALDO_ENABLE_REG), 0xF7);
  sw.turn_off();
  EXPECT_EQ(this->bus_.read(ALDO_ENABLE_REG), 0xF3);
}

TEST_F(Axp2101Test, SwitchHonoursTheRestoreMode) {
  AXP2101Switch sw;
  configure(sw, &this->axp_, "DLDO1");
  sw.set_restore_mode(switch_::SWITCH_ALWAYS_ON);

  sw.setup();
  ASSERT_FALSE(sw.is_failed());

  EXPECT_EQ(this->bus_.read(ALDO_ENABLE_REG) & 0x80, 0x80);
  EXPECT_TRUE(sw.state);
}

TEST_F(Axp2101Test, SwitchFailsOnAnUnknownRail) {
  AXP2101Switch sw;
  configure(sw, &this->axp_, "LDO9");
  sw.setup();

  EXPECT_TRUE(sw.is_failed());
  // A failed switch must not drive anything.
  EXPECT_FALSE(sw.state);
}

TEST_F(Axp2101Test, SwitchKeepsItsLastKnownStateWhenTheWriteFails) {
  AXP2101Switch sw;
  configure(sw, &this->axp_, "ALDO3");
  sw.setup();
  ASSERT_FALSE(sw.is_failed());

  sw.turn_on();
  ASSERT_TRUE(sw.state);

  this->bus_.failure = i2c::ERROR_TIMEOUT;
  sw.turn_off();

  // The write failed, so the rail's real state is unknown and the switch does
  // not claim it was turned off.
  EXPECT_TRUE(sw.state);
}

}  // namespace esphome::axp2101::testing
