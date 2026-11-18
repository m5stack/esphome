#pragma once

#include "esphome/components/axp_base/axp_base.h"
#include "esphome/components/output/float_output.h"
#include "esphome/core/component.h"

namespace esphome::axp192 {

/// Drives one power rail from a 0..1 level, using the rail's own voltage steps.
///
/// A level of 0 switches the rail off; anything else maps linearly onto
/// [min_voltage, max_voltage] and is snapped to the nearest voltage the rail can
/// actually produce. Linear is the right mapping here: an LED's current is
/// exponential in its forward voltage, so equal voltage steps give roughly equal
/// brightness ratios, which is what the eye perceives as even dimming.
class AXP192FloatOutput : public output::FloatOutput, public Component, public Parented<axp_base::AxpComponent> {
 public:
  /// Name of the rail as it appears in the model table.
  void set_rail_name(const char *name) { this->rail_name_ = name; }
  void set_min_voltage(uint16_t millivolt) { this->min_voltage_ = millivolt; }
  void set_max_voltage(uint16_t millivolt) { this->max_voltage_ = millivolt; }

  /// Same priority as the other PWM-style outputs in ESPHome.
  ///
  /// `LightState` runs its setup at HARDWARE - 1 so that an `ALWAYS_OFF` restore
  /// mode can switch the output off before anything else touches it. At the
  /// default DATA priority this setup() would run *after* that write, `rail_`
  /// would still be null, and the write would be dropped - leaving a rail that
  /// was on from a previous run on through the next boot.
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void setup() override;
  void dump_config() override;

 protected:
  void write_state(float state) override;

  const char *rail_name_{nullptr};
  const axp_base::AxpRail *rail_{nullptr};
  uint16_t min_voltage_{0};
  uint16_t max_voltage_{0};
};

}  // namespace esphome::axp192
