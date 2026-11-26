#include <gtest/gtest.h>

#include "esphome/components/m5_unit_ncir2/m5_unit_ncir2.h"
#include "esphome/core/application.h"
#include "../ncir2_bus.h"

namespace esphome::m5_unit_ncir2::testing {

class M5UnitNCIR2BinarySensorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->bus_.regs[REG_FIRMWARE_VERSION] = 2;
    this->bus_.regs[REG_BUTTON] = 1;
    this->device_.set_i2c_bus(&this->bus_);
    this->device_.set_i2c_address(0x5A);
    this->device_.set_button_binary_sensor(&this->button_);
  }

  void run_loop_at_(uint32_t now) {
    LoopBlockingGuard clock{nullptr, nullptr, now};
    this->device_.loop();
  }

  NCIR2Bus bus_;
  M5UnitNCIR2 device_;
  binary_sensor::BinarySensor button_;
};

TEST_F(M5UnitNCIR2BinarySensorTest, ButtonRegisterReadsZeroWhilePressed) {
  this->device_.setup();
  this->run_loop_at_(1000);
  EXPECT_FALSE(this->button_.state);
  this->bus_.regs[REG_BUTTON] = 0;
  this->run_loop_at_(1100);
  EXPECT_TRUE(this->button_.state);
  EXPECT_FALSE(this->bus_.locked);
}

TEST_F(M5UnitNCIR2BinarySensorTest, ButtonIsPolledAtMostEvery50Ms) {
  this->device_.setup();
  this->run_loop_at_(1000);
  this->bus_.regs[REG_BUTTON] = 0;
  this->run_loop_at_(1020);
  EXPECT_FALSE(this->button_.state);
  this->run_loop_at_(1050);
  EXPECT_TRUE(this->button_.state);
}

}  // namespace esphome::m5_unit_ncir2::testing
