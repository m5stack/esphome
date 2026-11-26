#include <gtest/gtest.h>

#include <cmath>
#include "esphome/components/m5_unit_ncir2/m5_unit_ncir2.h"
#include "esphome/core/application.h"
#include "../ncir2_bus.h"

namespace esphome::m5_unit_ncir2::testing {

class M5UnitNCIR2SensorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->bus_.regs[REG_FIRMWARE_VERSION] = 2;
    this->bus_.regs[REG_BUTTON] = 1;
    this->device_.set_i2c_bus(&this->bus_);
    this->device_.set_i2c_address(0x5A);
    this->device_.set_temperature_sensor(&this->temperature_);
    this->device_.set_internal_temperature_sensor(&this->internal_temperature_);
  }

  NCIR2Bus bus_;
  M5UnitNCIR2 device_;
  sensor::Sensor temperature_;
  sensor::Sensor internal_temperature_;
};

TEST_F(M5UnitNCIR2SensorTest, ReadsTemperaturesWithoutLockingTheBus) {
  this->bus_.set_int16(REG_TEMPERATURE, 2755);
  this->bus_.set_int16(REG_CHIP_TEMPERATURE, -1234);
  this->device_.setup();
  this->device_.update();
  EXPECT_FALSE(this->bus_.locked);
  EXPECT_FALSE(this->device_.is_failed());
  EXPECT_FLOAT_EQ(this->temperature_.state, 27.55f);
  EXPECT_FLOAT_EQ(this->internal_temperature_.state, -12.34f);
}

TEST_F(M5UnitNCIR2SensorTest, LockedBusSetsAWarningAndPublishesNothing) {
  this->device_.setup();
  this->bus_.locked = true;
  this->device_.update();
  EXPECT_TRUE(this->device_.status_has_warning());
  EXPECT_TRUE(std::isnan(this->temperature_.state));
}

}  // namespace esphome::m5_unit_ncir2::testing
