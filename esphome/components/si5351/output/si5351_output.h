#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"

#include "../si5351.h"


namespace esphome::si5351 {

/// Exposes a single Si5351 clock output as an ESPHome output so it can be
/// switched at runtime. A level above zero enables the clock, zero disables it.
class Si5351Output : public output::FloatOutput, public Component {
 public:
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_parent(Si5351Component *parent) { this->parent_ = parent; }
  void set_channel(uint8_t channel) { this->channel_ = channel; }

  bool set_frequency(double frequency);

 protected:
  void write_state(float state) override;

  Si5351Component *parent_{nullptr};
  uint8_t channel_{0};
};

} // namespace esphome::si5351

