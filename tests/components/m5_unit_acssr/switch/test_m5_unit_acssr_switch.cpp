#include <gtest/gtest.h>

#include "esphome/components/m5_unit_acssr/switch/m5_unit_acssr_switch.h"
#include "esphome/core/application.h"
#include "../acssr_bus.h"

namespace esphome::m5_unit_acssr::testing {

class M5UnitACSSRSwitchTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->bus_.restart();
    this->hub_.set_i2c_bus(&this->bus_);
    this->hub_.set_i2c_address(0x50);
    this->switch_.set_parent(&this->hub_);
    this->hub_.set_relay_switch(&this->switch_);
  }

  void TearDown() override {
    App.scheduler.cancel_interval(&this->hub_, uint32_t{0});
    App.scheduler.call(millis());
  }

  ACSSRBus bus_;
  M5UnitACSSR hub_;
  M5UnitACSSRRelaySwitch switch_;
};

TEST_F(M5UnitACSSRSwitchTest, RestoreModeSetsTheRelayAtBoot) {
  this->switch_.set_restore_mode(switch_::SWITCH_ALWAYS_ON);
  this->hub_.setup();
  EXPECT_EQ(this->bus_.regs[REG_RELAY], 1);
  EXPECT_TRUE(this->switch_.state);
}

TEST_F(M5UnitACSSRSwitchTest, DisabledRestoreKeepsTheUnitsState) {
  this->bus_.regs[REG_RELAY] = 1;
  this->switch_.set_restore_mode(switch_::SWITCH_RESTORE_DISABLED);
  this->hub_.setup();
  EXPECT_EQ(this->bus_.regs[REG_RELAY], 1);
  EXPECT_TRUE(this->switch_.state);
}

TEST_F(M5UnitACSSRSwitchTest, InvertedSwitchDrivesTheRelayTheOtherWay) {
  this->switch_.set_restore_mode(switch_::SWITCH_ALWAYS_OFF);
  this->switch_.set_inverted(true);
  this->hub_.setup();
  EXPECT_EQ(this->bus_.regs[REG_RELAY], 1);
  EXPECT_FALSE(this->switch_.state);
  this->switch_.turn_on();
  EXPECT_EQ(this->bus_.regs[REG_RELAY], 0);
  EXPECT_TRUE(this->switch_.state);
}

TEST_F(M5UnitACSSRSwitchTest, SwitchDrivesTheRelay) {
  this->switch_.set_restore_mode(switch_::SWITCH_ALWAYS_OFF);
  this->hub_.setup();
  this->switch_.turn_on();
  EXPECT_EQ(this->bus_.regs[REG_RELAY], 1);
  EXPECT_TRUE(this->switch_.state);
  this->switch_.turn_off();
  EXPECT_EQ(this->bus_.regs[REG_RELAY], 0);
  EXPECT_FALSE(this->switch_.state);
}

// The switch shows what the relay does, so a write the unit never got leaves it as it was
TEST_F(M5UnitACSSRSwitchTest, FailedWriteKeepsTheState) {
  this->switch_.set_restore_mode(switch_::SWITCH_ALWAYS_OFF);
  this->hub_.setup();
  this->bus_.absent = true;
  this->switch_.turn_on();
  EXPECT_FALSE(this->switch_.state);
  EXPECT_TRUE(this->hub_.status_has_warning());
}

}  // namespace esphome::m5_unit_acssr::testing
