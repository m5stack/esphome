#pragma once

#include "esphome/components/output/float_output.h"

#include "../m5stack_ioe1.h"

namespace esphome::m5stack_ioe1 {

class M5StackIOE1FloatOutput : public output::FloatOutput, public Component, public Parented<M5StackIOE1Component> {
 public:
  void setup() override;
  void dump_config() override;

  void set_channel(M5StackIOE1PWMChannel channel) { this->channel_ = channel; }
  void set_pwm_polarity(M5StackIOE1PWMPolarity polarity) { this->polarity_ = polarity; }

 protected:
  void write_state(float state) override;

  M5StackIOE1PWMChannel channel_{PWM_1};
  M5StackIOE1PWMPolarity polarity_{ACTIVE_HIGH};
};

}  // namespace esphome::m5stack_ioe1
