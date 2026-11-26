#include <gtest/gtest.h>

#include "esphome/components/m5_unit_acssr/m5_unit_acssr.h"
#include "esphome/core/application.h"
#include "acssr_bus.h"

namespace esphome::m5_unit_acssr::testing {

class Hub : public M5UnitACSSR {
 public:
  using M5UnitACSSR::retry_connect_;
};

class M5UnitACSSRTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->bus_.restart();
    this->hub_.set_i2c_bus(&this->bus_);
    this->hub_.set_i2c_address(0x50);
  }

  void TearDown() override {
    // The reconnect interval captures &hub_; drop it before the fixture dies
    App.scheduler.cancel_interval(&this->hub_, uint32_t{0});
    App.scheduler.call(millis());
  }

  ACSSRBus bus_;
  Hub hub_;
};

TEST_F(M5UnitACSSRTest, MissingUnitWarnsWithoutFailing) {
  this->bus_.absent = true;
  this->hub_.setup();
  EXPECT_FALSE(this->hub_.is_failed());
  EXPECT_TRUE(this->hub_.status_has_warning());
}

// A unit powered with the board starts its I2C target a few seconds after setup()
TEST_F(M5UnitACSSRTest, LateUnitGetsTheLedSetMeanwhile) {
  this->bus_.absent = true;
  this->hub_.setup();
  EXPECT_FALSE(this->hub_.write_led(0, 128, 255));
  this->hub_.retry_connect_();
  this->bus_.absent = false;
  this->hub_.retry_connect_();
  EXPECT_FALSE(this->hub_.status_has_warning());
  EXPECT_EQ(this->bus_.regs[REG_LED], 0);
  EXPECT_EQ(this->bus_.regs[REG_LED + 1], 128);
  EXPECT_EQ(this->bus_.regs[REG_LED + 2], 255);
}

TEST_F(M5UnitACSSRTest, UpdateConnectsOnceTheFastRetriesRunOut) {
  this->bus_.absent = true;
  this->hub_.setup();
  for (int attempt = 0; attempt < 10; attempt++)
    this->hub_.retry_connect_();
  this->bus_.absent = false;
  this->hub_.update();
  EXPECT_FALSE(this->hub_.status_has_warning());
}

TEST_F(M5UnitACSSRTest, RestartedUnitGetsItsStateBack) {
  this->hub_.setup();
  this->hub_.write_relay(true);
  this->hub_.write_led(255, 0, 0);
  this->bus_.restart();
  this->hub_.update();
  EXPECT_EQ(this->bus_.regs[REG_RELAY], 1);
  EXPECT_EQ(this->bus_.regs[REG_LED], 255);
}

TEST_F(M5UnitACSSRTest, UpdateLeavesAMatchingUnitAlone) {
  this->hub_.setup();
  this->hub_.write_relay(true);
  const unsigned writes = this->bus_.relay_writes;
  this->hub_.update();
  EXPECT_EQ(this->bus_.relay_writes, writes);
}

TEST_F(M5UnitACSSRTest, LostUnitIsRestoredWhenItAnswers) {
  this->hub_.setup();
  this->hub_.write_relay(true);
  this->bus_.absent = true;
  this->hub_.update();
  EXPECT_TRUE(this->hub_.status_has_warning());
  this->bus_.absent = false;
  this->bus_.restart();
  this->hub_.update();
  EXPECT_FALSE(this->hub_.status_has_warning());
  EXPECT_EQ(this->bus_.regs[REG_RELAY], 1);
}

}  // namespace esphome::m5_unit_acssr::testing
