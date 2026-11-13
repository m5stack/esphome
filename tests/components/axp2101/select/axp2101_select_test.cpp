// Tests for the AXP2101 select entities. Living in this subdirectory also tells
// the unit test harness to compile the `select` platform of this component.

#include <gtest/gtest.h>

#include "esphome/components/axp2101/select/axp2101_select.h"

#include "../axp2101_test.h"

namespace esphome::axp2101::testing {

namespace {

// Mirror of what the select platform passes as the option table.
const char *const CHARGE_CURRENT_OPTIONS[] = {
    "100mA", "125mA", "150mA", "175mA", "200mA", "300mA",  "400mA",
    "500mA", "600mA", "700mA", "800mA", "900mA", "1000mA",
};
const char *const POWER_KEY_OPTIONS[] = {"1s", "1.5s", "2s", "2.5s"};

}  // namespace

TEST_F(Axp2101Test, SelectPublishesTheStateTheChipIsIn) {
  // REG 62H code 10 is 400 mA, which is option 6 of the table above.
  this->bus_.poke(0x62, 10);

  AXP2101Select entity;
  entity.set_parent(&this->axp_);
  entity.set_select_name("charge_current");
  entity.traits.set_options_static(CHARGE_CURRENT_OPTIONS, 13);
  entity.setup();
  ASSERT_FALSE(entity.is_failed());

  EXPECT_EQ(entity.active_index(), 6u);
  EXPECT_EQ(entity.current_option(), "400mA");
}

TEST_F(Axp2101Test, SelectWritesTheCodeOfTheChosenOption) {
  AXP2101Select entity;
  entity.set_parent(&this->axp_);
  entity.set_select_name("charge_current");
  entity.traits.set_options_static(CHARGE_CURRENT_OPTIONS, 13);
  entity.setup();
  ASSERT_FALSE(entity.is_failed());

  entity.make_call().set_index(11).perform();

  EXPECT_EQ(this->bus_.read(0x62), 15);  // 900 mA
  EXPECT_EQ(entity.current_option(), "900mA");
}

TEST_F(Axp2101Test, SelectKeepsTheRestOfTheRegister) {
  // The power key threshold lives in bits 5:4 of REG 27H, whose other bits are
  // separate settings.
  this->bus_.poke(0x27, 0xCB);

  AXP2101Select entity;
  entity.set_parent(&this->axp_);
  entity.set_select_name("power_key_long_press");
  entity.traits.set_options_static(POWER_KEY_OPTIONS, 4);
  entity.setup();
  ASSERT_FALSE(entity.is_failed());

  entity.make_call().set_index(2).perform();

  EXPECT_EQ(this->bus_.read(0x27), (0xCB & 0xCF) | 0x20);
}

TEST_F(Axp2101Test, SelectPublishesNothingForAReservedCode) {
  // REG 62H code 0 is 0 mA, which the table deliberately leaves out so the
  // charger cannot be configured to never charge.
  this->bus_.poke(0x62, 0);

  AXP2101Select entity;
  entity.set_parent(&this->axp_);
  entity.set_select_name("charge_current");
  entity.traits.set_options_static(CHARGE_CURRENT_OPTIONS, 13);
  entity.setup();
  ASSERT_FALSE(entity.is_failed());

  EXPECT_FALSE(entity.has_state());
}

TEST_F(Axp2101Test, SelectFailsOnASettingTheChipDoesNotHave) {
  AXP2101Select entity;
  entity.set_parent(&this->axp_);
  entity.set_select_name("vbus_current");
  entity.traits.set_options_static(CHARGE_CURRENT_OPTIONS, 13);
  entity.setup();

  EXPECT_TRUE(entity.is_failed());
}

TEST_F(Axp2101Test, SelectFailsWhenTheOptionListDoesNotMatchTheTable) {
  AXP2101Select entity;
  entity.set_parent(&this->axp_);
  entity.set_select_name("charge_current");
  entity.traits.set_options_static(CHARGE_CURRENT_OPTIONS, 12);  // one short
  entity.setup();

  EXPECT_TRUE(entity.is_failed());
}

TEST_F(Axp2101Test, SelectKeepsItsStateWhenTheWriteFails) {
  this->bus_.poke(0x62, 10);  // 400 mA

  AXP2101Select entity;
  entity.set_parent(&this->axp_);
  entity.set_select_name("charge_current");
  entity.traits.set_options_static(CHARGE_CURRENT_OPTIONS, 13);
  entity.setup();
  ASSERT_FALSE(entity.is_failed());
  ASSERT_EQ(entity.active_index(), 6u);

  // The chip cannot be read either, so there is nothing to report and the last
  // known option stays.
  this->bus_.failure = i2c::ERROR_TIMEOUT;
  entity.make_call().set_index(11).perform();

  EXPECT_EQ(entity.active_index(), 6u);
}

}  // namespace esphome::axp2101::testing
