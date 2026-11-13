// Tests for the AXP2101 sensor entities. Living in this subdirectory also tells
// the unit test harness to compile the `sensor` platform of this component.

#include <gtest/gtest.h>

#include <cmath>

#include "esphome/components/axp2101/sensor/axp2101_sensor.h"

#include "../axp2101_test.h"

namespace esphome::axp2101::testing {

TEST_F(Axp2101Test, SensorChannelNamesMatchTheModelTable) {
  // The sensor platform refers to channels by these names. A rename on either
  // side would otherwise fail only at runtime on the device.
  for (const char *name : {"battery_voltage", "ts_voltage", "vbus_voltage", "system_voltage", "die_temperature"}) {
    EXPECT_NE(this->axp_.find_adc_channel(name), nullptr) << name;
  }
}

TEST_F(Axp2101Test, BatteryVoltageSensorPublishesVolts) {
  // A 4.0 V pack reports 4000 counts at 1 mV per count.
  this->bus_.poke(0x34, (4000 >> 8) & 0x3F);
  this->bus_.poke(0x35, 4000 & 0xFF);

  AXP2101AdcSensor sensor;
  sensor.set_parent(&this->axp_);
  sensor.set_channel_name("battery_voltage");
  sensor.setup();
  ASSERT_FALSE(sensor.is_failed());

  sensor.publish_from_chip();

  EXPECT_NEAR(sensor.state, 4.0f, 0.001f);
}

TEST_F(Axp2101Test, AdcSensorSwitchesItsChannelOn) {
  // REG 30H bit 2 is the VBUS channel, which the chip leaves off until asked.
  AXP2101AdcSensor sensor;
  sensor.set_parent(&this->axp_);
  sensor.set_channel_name("vbus_voltage");
  sensor.setup();
  ASSERT_FALSE(sensor.is_failed());

  EXPECT_EQ(this->bus_.read(0x30) & 0x04, 0x04);
  // Channels it does not use are left alone.
  EXPECT_EQ(this->bus_.read(0x30) & 0x10, 0x00);
}

TEST_F(Axp2101Test, AdcSensorFailsOnAnUnknownChannel) {
  AXP2101AdcSensor sensor;
  sensor.set_parent(&this->axp_);
  sensor.set_channel_name("battery_current");  // the AXP2101 has no such channel
  sensor.setup();

  EXPECT_TRUE(sensor.is_failed());

  // A failed sensor must not publish anything.
  this->bus_.poke(0x34, 0x0F);
  sensor.publish_from_chip();
  EXPECT_FALSE(sensor.has_state());
}

TEST_F(Axp2101Test, BatteryLevelSensorPublishesPercent) {
  this->bus_.poke(0xA4, 87);

  AXP2101SocSensor sensor;
  sensor.set_parent(&this->axp_);
  sensor.publish_from_chip();

  EXPECT_FLOAT_EQ(sensor.state, 87.0f);
}

TEST_F(Axp2101Test, SensorsStaySilentWhenTheBusFails) {
  this->bus_.failure = i2c::ERROR_NOT_ACKNOWLEDGED;

  AXP2101AdcSensor adc_sensor;
  adc_sensor.set_parent(&this->axp_);
  adc_sensor.set_channel_name("battery_voltage");
  adc_sensor.setup();
  adc_sensor.publish_from_chip();
  EXPECT_FALSE(adc_sensor.has_state());

  AXP2101SocSensor soc_sensor;
  soc_sensor.set_parent(&this->axp_);
  soc_sensor.publish_from_chip();
  EXPECT_FALSE(soc_sensor.has_state());
}

TEST_F(Axp2101Test, HubPollRefreshesStatusAndSensorsTogether) {
  this->bus_.poke(0x00, 0x28);  // battery present + VBUS good
  this->bus_.poke(0x34, (3900 >> 8) & 0x3F);
  this->bus_.poke(0x35, 3900 & 0xFF);
  this->bus_.poke(0xA4, 55);

  AXP2101AdcSensor voltage;
  voltage.set_parent(&this->axp_);
  voltage.set_channel_name("battery_voltage");
  voltage.setup();
  AXP2101SocSensor level;
  level.set_parent(&this->axp_);
  this->axp_.add_polled_entity(&voltage);
  this->axp_.add_polled_entity(&level);

  this->axp_.update();

  EXPECT_NEAR(voltage.state, 3.9f, 0.001f);
  EXPECT_FLOAT_EQ(level.state, 55.0f);

  const axp_base::AxpStatusFlag *present = this->axp_.find_status_flag("battery_present");
  ASSERT_NE(present, nullptr);
  EXPECT_TRUE(this->axp_.status_bit(present->bit));
}

}  // namespace esphome::axp2101::testing
