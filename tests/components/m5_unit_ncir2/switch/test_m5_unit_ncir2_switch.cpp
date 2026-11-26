#include <gtest/gtest.h>

#include "esphome/components/m5_unit_ncir2/switch/m5_unit_ncir2_switch.h"
#include "../ncir2_bus.h"

namespace esphome::m5_unit_ncir2::testing {

class M5UnitNCIR2SwitchTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->bus_.regs[REG_FIRMWARE_VERSION] = 2;
    this->hub_.set_i2c_bus(&this->bus_);
    this->hub_.set_i2c_address(0x5A);
    this->switch_.set_parent(&this->hub_);
    this->hub_.set_buzzer_switch(&this->switch_);
  }

  NCIR2Bus bus_;
  M5UnitNCIR2 hub_;
  M5UnitNCIR2BuzzerSwitch switch_;
};

TEST_F(M5UnitNCIR2SwitchTest, BootsWithTheUnitsBuzzerState) {
  this->bus_.regs[REG_BUZZER + 3] = 1;
  this->hub_.setup();
  EXPECT_TRUE(this->switch_.state);
}

TEST_F(M5UnitNCIR2SwitchTest, SwitchSoundsAndSilencesTheTone) {
  this->hub_.setup();
  this->switch_.turn_on();
  EXPECT_TRUE(this->switch_.state);
  EXPECT_EQ(this->bus_.get_uint16(REG_BUZZER), 4000);
  EXPECT_EQ(this->bus_.regs[REG_BUZZER + 2], 128);
  EXPECT_EQ(this->bus_.regs[REG_BUZZER + 3], 1);
  this->switch_.turn_off();
  EXPECT_FALSE(this->switch_.state);
  EXPECT_EQ(this->bus_.regs[REG_BUZZER + 3], 0);
}

// rtttl drives the buzzer through the output; the switch follows what the unit does
TEST_F(M5UnitNCIR2SwitchTest, SwitchFollowsOtherBuzzerWrites) {
  this->hub_.setup();
  this->hub_.write_buzzer(440, 128);
  EXPECT_TRUE(this->switch_.state);
  this->hub_.write_buzzer(440, 0);
  EXPECT_FALSE(this->switch_.state);
}

}  // namespace esphome::m5_unit_ncir2::testing
