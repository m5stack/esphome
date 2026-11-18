#pragma once

#include "esphome/components/axp_base/axp_base.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"

namespace esphome::axp192 {

/// A binary sensor backed by one of the chip's status flags.
///
/// Like the sensors, it never polls: the hub reads the status registers once per
/// interval and then asks every registered entity to publish.
class AXP192StatusBinarySensor : public binary_sensor::BinarySensor,
                                 public Component,
                                 public Parented<axp_base::AxpComponent>,
                                 public axp_base::AxpPolledEntity {
 public:
  /// Name of the flag as it appears in the model table.
  void set_flag_name(const char *name) { this->flag_name_ = name; }

  void setup() override;
  void dump_config() override;
  void publish_from_chip() override;

 protected:
  const char *flag_name_{nullptr};
  const axp_base::AxpStatusFlag *flag_{nullptr};
};

}  // namespace esphome::axp192
