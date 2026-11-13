// Tests for the AXP2101 binary sensor entities. Living in this subdirectory
// also tells the unit test harness to compile the `binary_sensor` platform of
// this component.

#include <gtest/gtest.h>

#include "esphome/components/axp2101/binary_sensor/axp2101_binary_sensor.h"

#include "../axp2101_test.h"

namespace esphome::axp2101::testing {

TEST_F(Axp2101Test, StatusFlagNamesMatchTheConfigurationKeys) {
  // The binary_sensor platform uses the configuration key as the flag name, so
  // a rename on either side must not go unnoticed.
  for (const char *name : {"battery_present", "battery_charging", "battery_discharging", "vbus_good",
                           "battery_active_mode", "thermal_regulation", "current_limit"}) {
    EXPECT_NE(this->axp_.find_status_flag(name), nullptr) << name;
  }
  // The chip reports neither of these as a status bit.
  EXPECT_EQ(this->axp_.find_status_flag("vbus_present"), nullptr);
  EXPECT_EQ(this->axp_.find_status_flag("over_temperature"), nullptr);
}

TEST_F(Axp2101Test, StatusBinarySensorPublishesItsFlag) {
  AXP2101StatusBinarySensor sensor;
  sensor.set_parent(&this->axp_);
  sensor.set_flag_name("vbus_good");
  sensor.setup();
  ASSERT_FALSE(sensor.is_failed());

  this->bus_.poke(0x00, 0x20);
  ASSERT_TRUE(this->axp_.update_status());
  sensor.publish_from_chip();
  EXPECT_TRUE(sensor.state);

  this->bus_.poke(0x00, 0x00);
  ASSERT_TRUE(this->axp_.update_status());
  sensor.publish_from_chip();
  EXPECT_FALSE(sensor.state);
}

TEST_F(Axp2101Test, StatusBinarySensorFailsOnAnUnknownFlag) {
  AXP2101StatusBinarySensor sensor;
  sensor.set_parent(&this->axp_);
  sensor.set_flag_name("vbus_present");  // the chip has no such flag
  sensor.setup();

  EXPECT_TRUE(sensor.is_failed());

  this->bus_.poke(0x00, 0xFF);
  ASSERT_TRUE(this->axp_.update_status());
  sensor.publish_from_chip();
  EXPECT_FALSE(sensor.has_state());
}

TEST_F(Axp2101Test, ChargingBinarySensorTracksTheChargeBits) {
  AXP2101StatusBinarySensor charging;
  charging.set_parent(&this->axp_);
  charging.set_flag_name("battery_charging");
  charging.setup();
  ASSERT_FALSE(charging.is_failed());

  this->bus_.poke(0x01, 0x20);  // bits 6:5 = 01, charging
  ASSERT_TRUE(this->axp_.update_status());
  charging.publish_from_chip();
  EXPECT_TRUE(charging.state);

  this->bus_.poke(0x01, 0x40);  // bits 6:5 = 10, discharging
  ASSERT_TRUE(this->axp_.update_status());
  charging.publish_from_chip();
  EXPECT_FALSE(charging.state);

  this->bus_.poke(0x01, 0x00);  // standby
  ASSERT_TRUE(this->axp_.update_status());
  charging.publish_from_chip();
  EXPECT_FALSE(charging.state);
}

TEST_F(Axp2101Test, HubPublishesEveryRegisteredEntityFromOnePoll) {
  AXP2101StatusBinarySensor charging;
  charging.set_parent(&this->axp_);
  charging.set_flag_name("battery_charging");
  charging.setup();
  AXP2101StatusBinarySensor present;
  present.set_parent(&this->axp_);
  present.set_flag_name("battery_present");
  present.setup();
  this->axp_.add_polled_entity(&charging);
  this->axp_.add_polled_entity(&present);

  this->bus_.poke(0x00, 0x08);  // battery present
  this->bus_.poke(0x01, 0x20);  // charging

  this->axp_.update();

  EXPECT_TRUE(charging.state);
  EXPECT_TRUE(present.state);
}

}  // namespace esphome::axp2101::testing
