#pragma once

#include <cmath>

#include "esphome/components/m5_unit_ncir2/m5_unit_ncir2.h"
#include "esphome/components/output/float_output.h"
#include "esphome/core/helpers.h"

namespace esphome::m5_unit_ncir2 {

class M5UnitNCIR2Buzzer : public output::FloatOutput, public Parented<M5UnitNCIR2> {
 public:
  explicit M5UnitNCIR2Buzzer(uint16_t frequency) : frequency_(frequency) {}

  void update_frequency(float frequency) override {
    this->frequency_ = static_cast<uint16_t>(clamp(frequency, 20.0f, 20000.0f));
    // A silent buzzer takes the new frequency with its next level
    if (this->duty_ != 0)
      this->parent_->write_buzzer(this->frequency_, this->duty_);
  }

 protected:
  void write_state(float state) override {
    this->duty_ = static_cast<uint8_t>(lroundf(state * 255.0f));
    this->parent_->write_buzzer(this->frequency_, this->duty_);
  }

  uint16_t frequency_;
  uint8_t duty_{0};
};

}  // namespace esphome::m5_unit_ncir2
