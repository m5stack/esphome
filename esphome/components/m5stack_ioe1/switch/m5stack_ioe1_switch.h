#pragma once

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"

#include "../m5stack_ioe1.h"

namespace esphome::m5stack_ioe1 {

class M5StackIOE1AW8737ASwitch : public switch_::Switch, public Component, public Parented<M5StackIOE1Component> {
 public:
  void setup() override;
  void dump_config() override;

  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_pulse_num(uint8_t pulse_num) { this->pulse_num_ = pulse_num; }

 protected:
  void write_state(bool state) override;

  uint8_t pin_{0};
  uint8_t pulse_num_{0};
};

}  // namespace esphome::m5stack_ioe1
