// Tests for the AXP2101 rail outputs. Living in this subdirectory also tells
// the unit test harness to compile the `output` platform of this component.

#include <gtest/gtest.h>

#include "esphome/components/axp2101/output/axp2101_output.h"

#include "../axp2101_test.h"

namespace esphome::axp2101::testing {

namespace {

constexpr uint8_t DCDC_ENABLE_REG = 0x80;
constexpr uint8_t DCDC3_VSEL_REG = 0x84;

void configure(AXP2101FloatOutput &out, axp_base::AxpComponent *parent, const char *rail, uint16_t min_voltage,
               uint16_t max_voltage) {
  out.set_parent(parent);
  out.set_rail_name(rail);
  out.set_min_voltage(min_voltage);
  out.set_max_voltage(max_voltage);
}

}  // namespace

TEST_F(Axp2101Test, OutputMapsTheLevelOntoTheVoltageRange) {
  AXP2101FloatOutput out;
  configure(out, &this->axp_, "DCDC3", 2800, 3300);
  out.setup();
  ASSERT_FALSE(out.is_failed());

  // Six levels between 2.8 and 3.3 V: full level is the top of the range.
  out.set_level(1.0f);
  EXPECT_EQ(this->bus_.read(DCDC_ENABLE_REG) & 0x04, 0x04);
  EXPECT_EQ((this->bus_.read(DCDC3_VSEL_REG) & 0x7F), 105);  // 3300 mV

  out.set_level(0.5f);
  EXPECT_EQ((this->bus_.read(DCDC3_VSEL_REG) & 0x7F), 103);  // 3100 mV, nearest step

  out.set_level(0.01f);
  EXPECT_EQ((this->bus_.read(DCDC3_VSEL_REG) & 0x7F), 100);  // 2800 mV
}

TEST_F(Axp2101Test, OutputTurnsTheRailOffAtZero) {
  AXP2101FloatOutput out;
  configure(out, &this->axp_, "DCDC3", 2800, 3300);
  out.setup();
  ASSERT_FALSE(out.is_failed());

  this->bus_.poke(DCDC_ENABLE_REG, 0x04);
  out.set_level(0.0f);

  // A rail parked at its lowest voltage is still lit, so zero switches it off.
  EXPECT_EQ(this->bus_.read(DCDC_ENABLE_REG) & 0x04, 0);

  // The set point stays where it was, so the next level comes back at once.
  out.set_level(1.0f);
  EXPECT_EQ(this->bus_.read(DCDC_ENABLE_REG) & 0x04, 0x04);
}

TEST_F(Axp2101Test, OutputSetsTheVoltageBeforeEnablingTheRail) {
  AXP2101FloatOutput out;
  configure(out, &this->axp_, "ALDO1", 2000, 3000);
  out.setup();
  ASSERT_FALSE(out.is_failed());

  // Coming out of a disabled rail must not briefly apply a stale set point,
  // so the voltage write has to happen first.
  this->bus_.poke(0x90, 0x00);
  this->bus_.poke(0x92, 3);  // 800 mV left behind by something else

  out.set_level(1.0f);

  EXPECT_EQ((this->bus_.read(0x92) & 0x1F), 25);  // 3000 mV
  EXPECT_EQ(this->bus_.read(0x90) & 0x01, 0x01);
}

TEST_F(Axp2101Test, OutputSkipsARepeatedLevel) {
  AXP2101FloatOutput out;
  configure(out, &this->axp_, "DCDC3", 2800, 3300);
  out.setup();
  ASSERT_FALSE(out.is_failed());

  out.set_level(0.6f);
  const uint8_t first = this->bus_.read(DCDC3_VSEL_REG);

  // FloatOutput drops a level it has already applied, so a light fading through
  // the same value does not talk to the bus again.
  out.set_level(0.6f);
  EXPECT_EQ(this->bus_.read(DCDC3_VSEL_REG), first);
}

TEST_F(Axp2101Test, OutputFailsWhenTheRangeHoldsTooFewVoltages) {
  AXP2101FloatOutput out;
  // 2.95 to 3.05 V on a rail with 100 mV steps holds just 3.0 V, which cannot
  // be dimmed at all.
  configure(out, &this->axp_, "ALDO1", 2950, 3050);
  out.setup();

  EXPECT_TRUE(out.is_failed());
}

TEST_F(Axp2101Test, OutputFailsWhenTheRangeIsInverted) {
  AXP2101FloatOutput out;
  configure(out, &this->axp_, "ALDO1", 3000, 2000);
  out.setup();

  EXPECT_TRUE(out.is_failed());
}

TEST_F(Axp2101Test, OutputFailsOnAnUnknownRail) {
  AXP2101FloatOutput out;
  configure(out, &this->axp_, "DCDC9", 2800, 3300);
  out.setup();

  EXPECT_TRUE(out.is_failed());
  out.set_level(1.0f);
  EXPECT_EQ(this->bus_.read(DCDC_ENABLE_REG) & 0x04, 0);
}

TEST_F(Axp2101Test, OutputEnablesDvmOnlyOnTheConvertersThatHaveIt) {
  AXP2101FloatOutput dcdc;
  configure(dcdc, &this->axp_, "DCDC3", 2800, 3300);
  dcdc.set_dvm(true);
  dcdc.setup();
  ASSERT_FALSE(dcdc.is_failed());
  EXPECT_EQ(this->bus_.read(DCDC3_VSEL_REG) & 0x80, 0x80);

  // An LDO has no DVM bit; asking for one is a warning, not a failure.
  AXP2101FloatOutput ldo;
  configure(ldo, &this->axp_, "ALDO1", 2000, 3000);
  ldo.set_dvm(true);
  ldo.setup();
  EXPECT_FALSE(ldo.is_failed());
  EXPECT_EQ(this->bus_.read(0x92) & 0x80, 0);
}

TEST_F(Axp2101Test, OutputLeavesTheRailOffWhenTheVoltageWriteFails) {
  AXP2101FloatOutput out;
  configure(out, &this->axp_, "DCDC3", 2800, 3300);
  out.setup();
  ASSERT_FALSE(out.is_failed());

  this->bus_.failure = i2c::ERROR_TIMEOUT;
  out.set_level(1.0f);

  // There is no point switching a rail on at whatever voltage it happens to
  // hold, so the rail stays off when the set point could not be written.
  EXPECT_EQ(this->bus_.read(DCDC_ENABLE_REG) & 0x04, 0);
}

TEST_F(Axp2101Test, OutputKeepsDvmWhileItChangesTheVoltage) {
  AXP2101FloatOutput out;
  configure(out, &this->axp_, "DCDC3", 2800, 3300);
  out.set_dvm(true);
  out.setup();
  ASSERT_FALSE(out.is_failed());

  out.set_level(0.2f);

  // The level write must not clear the DVM bit it relies on.
  EXPECT_EQ(this->bus_.read(DCDC3_VSEL_REG) & 0x80, 0x80);
}

}  // namespace esphome::axp2101::testing
