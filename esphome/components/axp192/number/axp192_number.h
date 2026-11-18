#pragma once

#include "esphome/components/axp_base/axp_base.h"
#include "esphome/components/number/number.h"
#include "esphome/core/component.h"

namespace esphome::axp192 {

/// Drives one of the chip's linearly scaled settings.
///
/// A value that falls between two codes is rounded to the nearest one, so the
/// number always shows what the chip can actually hold.
class AXP192Number : public number::Number, public Component, public Parented<axp_base::AxpComponent> {
 public:
  /// Name of the setting as it appears in the model table.
  void set_number_name(const char *name) { this->number_name_ = name; }

  void setup() override;
  void dump_config() override;

 protected:
  void control(float value) override;

  /// Publish the value the chip is actually set to.
  void publish_current_value_();

  const char *number_name_{nullptr};
  const axp_base::AxpNumber *number_{nullptr};
};

}  // namespace esphome::axp192
