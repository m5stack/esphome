#include <gtest/gtest.h>

#include "esphome/components/m5_unit_ncir2/number/m5_unit_ncir2_number.h"
#include "esphome/core/application.h"
#include "../ncir2_bus.h"

namespace esphome::m5_unit_ncir2::testing {

class SavingHub : public M5UnitNCIR2 {
 public:
  using M5UnitNCIR2::save_settings_;
  using M5UnitNCIR2::saving_;
};

class M5UnitNCIR2NumberTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->bus_.regs[REG_FIRMWARE_VERSION] = 2;
    // Factory alarms: low 23 °C at 1000 Hz, high 35 °C at 4000 Hz, both every second at duty 51
    this->bus_.set_int16(REG_ALARM_TEMPERATURE, 2300);
    this->bus_.set_int16(REG_ALARM_TEMPERATURE + 2, 3500);
    this->bus_.set_int16(REG_ALARM_BUZZER, 1000);
    this->bus_.set_int16(REG_ALARM_BUZZER + 2, 1000);
    this->bus_.regs[REG_ALARM_BUZZER + 4] = 51;
    this->bus_.set_int16(REG_ALARM_BUZZER + 5, 4000);
    this->hub_.set_i2c_bus(&this->bus_);
    this->hub_.set_i2c_address(0x5A);
    this->add_(Setting::SETTING_LOW_ALARM_TEMPERATURE, this->low_temperature_);
    this->add_(Setting::SETTING_HIGH_ALARM_FREQUENCY, this->high_frequency_);
    this->add_(Setting::SETTING_LOW_ALARM_DUTY, this->low_duty_);
    this->add_(Setting::SETTING_BUZZER_FREQUENCY, this->buzzer_frequency_);
  }

  void TearDown() override {
    // The save timers capture &hub_; drop them before the fixture dies
    App.scheduler.cancel_timeout(&this->hub_, uint32_t{0});
    App.scheduler.cancel_timeout(&this->hub_, uint32_t{1});
    App.scheduler.call(millis());
  }

  void add_(Setting setting, M5UnitNCIR2Number &number) {
    number.set_parent(&this->hub_);
    this->hub_.set_setting_number(setting, &number);
  }

  NCIR2Bus bus_;
  SavingHub hub_;
  M5UnitNCIR2Number low_temperature_{Setting::SETTING_LOW_ALARM_TEMPERATURE};
  M5UnitNCIR2Number high_frequency_{Setting::SETTING_HIGH_ALARM_FREQUENCY};
  M5UnitNCIR2Number low_duty_{Setting::SETTING_LOW_ALARM_DUTY};
  M5UnitNCIR2Number buzzer_frequency_{Setting::SETTING_BUZZER_FREQUENCY};
};

TEST_F(M5UnitNCIR2NumberTest, NumbersBootWithTheUnitsValues) {
  this->hub_.setup();
  EXPECT_FALSE(this->hub_.is_failed());
  EXPECT_FALSE(this->bus_.locked);
  EXPECT_FLOAT_EQ(this->low_temperature_.state, 23.0f);
  EXPECT_FLOAT_EQ(this->high_frequency_.state, 4000.0f);
  EXPECT_FLOAT_EQ(this->low_duty_.state, 20.0f);
  EXPECT_FLOAT_EQ(this->buzzer_frequency_.state, 4000.0f);
}

TEST_F(M5UnitNCIR2NumberTest, YamlSettingsAreWhatTheNumbersShow) {
  this->hub_.set_alarm_temperature(0, -2000);
  this->hub_.setup();
  EXPECT_FLOAT_EQ(this->low_temperature_.state, -20.0f);
}

TEST_F(M5UnitNCIR2NumberTest, ControlWritesTheAlarmRegister) {
  this->hub_.setup();
  this->low_temperature_.make_call().set_value(-12.5f).perform();
  EXPECT_EQ(static_cast<int16_t>(this->bus_.get_uint16(REG_ALARM_TEMPERATURE)), -1250);
  EXPECT_FLOAT_EQ(this->low_temperature_.state, -12.5f);
  this->high_frequency_.make_call().set_value(2500).perform();
  EXPECT_EQ(this->bus_.get_uint16(REG_ALARM_BUZZER + 5), 2500);
  this->low_duty_.make_call().set_value(100).perform();
  EXPECT_EQ(this->bus_.regs[REG_ALARM_BUZZER + 4], 255);
  // Saved later, once the numbers are still
  EXPECT_EQ(this->bus_.regs[REG_SAVE_SETTINGS], 0);
  EXPECT_FALSE(this->bus_.locked);
}

TEST_F(M5UnitNCIR2NumberTest, SaveWritesTheFlagAndHoldsTrafficBack) {
  this->hub_.setup();
  this->hub_.save_settings_();
  EXPECT_EQ(this->bus_.regs[REG_SAVE_SETTINGS], 1);
  EXPECT_TRUE(this->hub_.saving_);
  EXPECT_FALSE(this->hub_.write_led(1, 2, 3));
  EXPECT_FALSE(this->hub_.control_setting(Setting::SETTING_LOW_ALARM_TEMPERATURE, 10.0f));
  EXPECT_EQ(this->bus_.get_uint16(REG_ALARM_TEMPERATURE), 2300);
}

TEST_F(M5UnitNCIR2NumberTest, BuzzerToneOnlyReachesTheUnitWhileItSounds) {
  this->hub_.setup();
  this->buzzer_frequency_.make_call().set_value(880).perform();
  EXPECT_FLOAT_EQ(this->buzzer_frequency_.state, 880.0f);
  EXPECT_EQ(this->bus_.buzzer_writes, 0u);
  EXPECT_TRUE(this->hub_.control_buzzer(true));
  EXPECT_EQ(this->bus_.get_uint16(REG_BUZZER), 880);
  this->buzzer_frequency_.make_call().set_value(440).perform();
  EXPECT_EQ(this->bus_.get_uint16(REG_BUZZER), 440);
  EXPECT_EQ(this->bus_.regs[REG_BUZZER + 3], 1);
}

}  // namespace esphome::m5_unit_ncir2::testing
