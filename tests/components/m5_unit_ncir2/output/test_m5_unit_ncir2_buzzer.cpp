#include <gtest/gtest.h>

#include "esphome/components/m5_unit_ncir2/output/m5_unit_ncir2_buzzer.h"
#include "../ncir2_bus.h"

namespace esphome::m5_unit_ncir2::testing {

class M5UnitNCIR2BuzzerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->bus_.regs[REG_FIRMWARE_VERSION] = 2;
    this->hub_.set_i2c_bus(&this->bus_);
    this->hub_.set_i2c_address(0x5A);
    this->hub_.setup();
    this->buzzer_.set_parent(&this->hub_);
  }

  NCIR2Bus bus_;
  M5UnitNCIR2 hub_;
  M5UnitNCIR2Buzzer buzzer_{4000};
};

TEST_F(M5UnitNCIR2BuzzerTest, LevelSetsTheDutyAndEnablesTheBuzzer) {
  this->buzzer_.set_level(0.5f);
  EXPECT_EQ(this->bus_.get_uint16(REG_BUZZER), 4000);
  EXPECT_EQ(this->bus_.regs[REG_BUZZER + 2], 128);
  EXPECT_EQ(this->bus_.regs[REG_BUZZER + 3], 1);
  this->buzzer_.set_level(0.0f);
  EXPECT_EQ(this->bus_.regs[REG_BUZZER + 3], 0);
}

// rtttl changes the frequency, then sets the level for each note
TEST_F(M5UnitNCIR2BuzzerTest, FrequencyChangeWaitsForTheNextLevelWhileSilent) {
  this->buzzer_.update_frequency(880.0f);
  EXPECT_EQ(this->bus_.buzzer_writes, 0u);
  this->buzzer_.set_level(0.5f);
  EXPECT_EQ(this->bus_.get_uint16(REG_BUZZER), 880);
  this->buzzer_.update_frequency(440.0f);
  EXPECT_EQ(this->bus_.get_uint16(REG_BUZZER), 440);
  EXPECT_EQ(this->bus_.buzzer_writes, 2u);
}

}  // namespace esphome::m5_unit_ncir2::testing
