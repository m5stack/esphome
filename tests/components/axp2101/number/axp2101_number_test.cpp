// Tests for the AXP2101 number entities. Living in this subdirectory also tells
// the unit test harness to compile the `number` platform of this component.

#include <gtest/gtest.h>

#include "esphome/components/axp2101/number/axp2101_number.h"

#include "../axp2101_test.h"

namespace esphome::axp2101::testing {

TEST_F(Axp2101Test, NumberPublishesTheValueTheChipIsSetTo) {
  // REG 15H code 1 is 3.88 V + 80 mV = 3960 mV.
  this->bus_.poke(0x15, 0x01);

  AXP2101Number entity;
  entity.set_parent(&this->axp_);
  entity.set_number_name("input_voltage_limit");
  entity.setup();
  ASSERT_FALSE(entity.is_failed());

  EXPECT_FLOAT_EQ(entity.state, 3960.0f);
}

TEST_F(Axp2101Test, NumberWritesTheCodeForItsValue) {
  AXP2101Number entity;
  entity.set_parent(&this->axp_);
  entity.set_number_name("input_voltage_limit");
  entity.setup();
  ASSERT_FALSE(entity.is_failed());

  entity.make_call().set_value(4600.0f).perform();

  EXPECT_EQ(this->bus_.read(0x15), 9);  // 3880 + 9 * 80
  EXPECT_FLOAT_EQ(entity.state, 4600.0f);
}

TEST_F(Axp2101Test, NumberRoundsAValueBetweenSteps) {
  AXP2101Number entity;
  entity.set_parent(&this->axp_);
  entity.set_number_name("input_voltage_limit");
  entity.setup();
  ASSERT_FALSE(entity.is_failed());

  entity.make_call().set_value(3930.0f).perform();

  // 3930 mV is nearer 3960 than 3880, and the entity reports what was applied.
  EXPECT_EQ(this->bus_.read(0x15), 1);
  EXPECT_FLOAT_EQ(entity.state, 3960.0f);
}

TEST_F(Axp2101Test, NumberKeepsTheOtherThresholdInTheSameRegister) {
  // REG 1AH bits 7:4 are the warning threshold and bits 3:0 the shutdown one.
  this->bus_.poke(0x1A, 0x03);

  AXP2101Number entity;
  entity.set_parent(&this->axp_);
  entity.set_number_name("battery_low_warning");
  entity.setup();
  ASSERT_FALSE(entity.is_failed());

  entity.make_call().set_value(20.0f).perform();

  EXPECT_EQ(this->bus_.read(0x1A), 0xF3);
  EXPECT_FLOAT_EQ(entity.state, 20.0f);
}

TEST_F(Axp2101Test, NumberPublishesNothingForAReservedCode) {
  // REG 61H codes 9..15 are reserved by the datasheet.
  this->bus_.poke(0x61, 0x0A);

  AXP2101Number entity;
  entity.set_parent(&this->axp_);
  entity.set_number_name("precharge_current");
  entity.setup();
  ASSERT_FALSE(entity.is_failed());

  EXPECT_FALSE(entity.has_state());
}

TEST_F(Axp2101Test, NumberFailsOnASettingTheChipDoesNotHave) {
  AXP2101Number entity;
  entity.set_parent(&this->axp_);
  entity.set_number_name("battery_capacity");
  entity.setup();

  EXPECT_TRUE(entity.is_failed());
}

TEST_F(Axp2101Test, NumberKeepsItsValueWhenTheWriteFails) {
  this->bus_.poke(0x15, 0x01);  // 3960 mV

  AXP2101Number entity;
  entity.set_parent(&this->axp_);
  entity.set_number_name("input_voltage_limit");
  entity.setup();
  ASSERT_FALSE(entity.is_failed());
  ASSERT_FLOAT_EQ(entity.state, 3960.0f);

  this->bus_.failure = i2c::ERROR_TIMEOUT;
  entity.make_call().set_value(4600.0f).perform();

  EXPECT_FLOAT_EQ(entity.state, 3960.0f);
}

}  // namespace esphome::axp2101::testing
