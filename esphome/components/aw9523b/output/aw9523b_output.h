#pragma once

#include "esphome/components/output/float_output.h"
#include "esphome/core/gpio.h"

#include "../aw9523b.h"

namespace esphome::aw9523b {

class AW9523BFloatOutput : public output::FloatOutput, public Component, public Parented<AW9523BComponent> {
 public:
  void setup() override;
  void dump_config() override;
  void set_pin(uint8_t pin) { this->pin_ = pin; }

 protected:
  void write_state(float state) override;

  /// Pin number from the configuration
  uint8_t pin_{0};
};

}  // namespace esphome::aw9523b
