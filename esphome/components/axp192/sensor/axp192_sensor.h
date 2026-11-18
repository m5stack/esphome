#pragma once

#include "esphome/components/axp_base/axp_base.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome::axp192 {

/// A sensor entity fed by the PMIC.
///
/// The hub owns the polling: once per `update_interval` it refreshes its own
/// cache and then asks every registered entity to publish. A sensor therefore
/// never polls on its own, so adding entities costs no extra bus traffic and the
/// I2C timing stays under the hub's control.
class AXP192Sensor : public sensor::Sensor,
                     public Component,
                     public Parented<axp_base::AxpComponent>,
                     public axp_base::AxpPolledEntity {
 public:
  void setup() override {}
  void dump_config() override;

  void publish_from_chip() override;

 protected:
  /// Fill `value` with what should be published. Return false to publish
  /// nothing, which is what happens on a bus error.
  virtual bool read_value_(float &value) = 0;
};

/// A sensor backed by one ADC channel of the chip.
class AXP192AdcSensor : public AXP192Sensor {
 public:
  /// Name of the channel as it appears in the model table.
  void set_channel_name(const char *name) { this->channel_name_ = name; }

  void setup() override;

 protected:
  bool read_value_(float &value) override;

  const char *channel_name_{nullptr};
  const axp_base::AxpAdcChannel *channel_{nullptr};
};

}  // namespace esphome::axp192
