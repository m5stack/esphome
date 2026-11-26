#include <gtest/gtest.h>

#include "esphome/components/m5_unit_ncir2/m5_unit_ncir2.h"
#include "esphome/core/application.h"
#include "ncir2_bus.h"

namespace esphome::m5_unit_ncir2::testing {

class M5UnitNCIR2Test : public ::testing::Test {
 protected:
  void SetUp() override {
    this->bus_.regs[REG_FIRMWARE_VERSION] = 2;
    this->bus_.regs[REG_BUTTON] = 1;
    this->device_.set_i2c_bus(&this->bus_);
    this->device_.set_i2c_address(0x5A);
  }

  NCIR2Bus bus_;
  M5UnitNCIR2 device_;
};

TEST_F(M5UnitNCIR2Test, MissingUnitFailsSetup) {
  this->bus_.absent = true;
  this->device_.setup();
  EXPECT_TRUE(this->device_.is_failed());
}

TEST_F(M5UnitNCIR2Test, EmissivityIsOnlyWrittenWhenItDiffers) {
  this->bus_.set_int16(REG_EMISSIVITY, static_cast<int16_t>(62258));
  this->device_.set_emissivity(62258);
  this->device_.setup();
  EXPECT_EQ(this->bus_.emissivity_writes, 0u);

  M5UnitNCIR2 other;
  other.set_i2c_bus(&this->bus_);
  other.set_i2c_address(0x5A);
  other.set_emissivity(63569);
  other.setup();
  EXPECT_EQ(this->bus_.emissivity_writes, 1u);
  EXPECT_EQ(this->bus_.get_uint16(REG_EMISSIVITY), 63569);
  EXPECT_FALSE(this->bus_.locked);
}

TEST_F(M5UnitNCIR2Test, AlarmSettingsLandInTheirRegisters) {
  this->device_.set_alarm_temperature(0, -2000);
  this->device_.set_alarm_buzzer(0, 4000, 100, 0);
  this->device_.set_alarm_temperature(1, 3000);
  this->device_.set_alarm_color(1, 0, 128, 0);
  this->device_.set_alarm_buzzer(1, 2000, 300, 76);
  this->device_.setup();
  EXPECT_FALSE(this->device_.is_failed());
  EXPECT_EQ(static_cast<int16_t>(this->bus_.get_uint16(REG_ALARM_TEMPERATURE)), -2000);
  EXPECT_EQ(this->bus_.get_uint16(REG_ALARM_TEMPERATURE + 2), 3000);
  EXPECT_EQ(this->bus_.get_uint16(REG_ALARM_BUZZER), 4000);
  EXPECT_EQ(this->bus_.get_uint16(REG_ALARM_BUZZER + 2), 100);
  EXPECT_EQ(this->bus_.regs[REG_ALARM_BUZZER + 4], 0);
  EXPECT_EQ(this->bus_.regs[REG_ALARM_LED + 3], 0);
  EXPECT_EQ(this->bus_.regs[REG_ALARM_LED + 4], 128);
  EXPECT_EQ(this->bus_.regs[REG_ALARM_LED + 5], 0);
  EXPECT_EQ(this->bus_.get_uint16(REG_ALARM_BUZZER + 5), 2000);
  EXPECT_EQ(this->bus_.get_uint16(REG_ALARM_BUZZER + 7), 300);
  EXPECT_EQ(this->bus_.regs[REG_ALARM_BUZZER + 9], 76);
}

TEST_F(M5UnitNCIR2Test, UnsetAlarmSettingsKeepTheUnitsOwn) {
  this->bus_.set_int16(REG_ALARM_TEMPERATURE, 2300);
  this->bus_.regs[REG_ALARM_LED] = 0x20;
  this->device_.set_alarm_temperature(1, 3000);
  this->device_.setup();
  EXPECT_EQ(this->bus_.get_uint16(REG_ALARM_TEMPERATURE), 2300);
  EXPECT_EQ(this->bus_.regs[REG_ALARM_LED], 0x20);
}

TEST_F(M5UnitNCIR2Test, LedAndBuzzerWrites) {
  this->device_.setup();
  EXPECT_TRUE(this->device_.write_led(10, 20, 30));
  EXPECT_EQ(this->bus_.regs[REG_LED], 10);
  EXPECT_EQ(this->bus_.regs[REG_LED + 1], 20);
  EXPECT_EQ(this->bus_.regs[REG_LED + 2], 30);
  EXPECT_TRUE(this->device_.write_buzzer(4000, 128));
  EXPECT_EQ(this->bus_.get_uint16(REG_BUZZER), 4000);
  EXPECT_EQ(this->bus_.regs[REG_BUZZER + 2], 128);
  EXPECT_EQ(this->bus_.regs[REG_BUZZER + 3], 1);
  EXPECT_TRUE(this->device_.write_buzzer(4000, 0));
  EXPECT_EQ(this->bus_.regs[REG_BUZZER + 3], 0);
}

TEST_F(M5UnitNCIR2Test, FailedUnitRefusesWrites) {
  this->bus_.absent = true;
  this->device_.setup();
  this->bus_.absent = false;
  EXPECT_FALSE(this->device_.write_led(1, 2, 3));
  EXPECT_FALSE(this->device_.write_buzzer(4000, 128));
  EXPECT_EQ(this->bus_.buzzer_writes, 0u);
}

}  // namespace esphome::m5_unit_ncir2::testing
