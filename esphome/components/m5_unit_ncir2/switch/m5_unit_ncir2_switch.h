#pragma once

#include "esphome/components/m5_unit_ncir2/m5_unit_ncir2.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/helpers.h"

namespace esphome::m5_unit_ncir2 {

class M5UnitNCIR2BuzzerSwitch : public switch_::Switch, public Parented<M5UnitNCIR2> {
 protected:
  void write_state(bool state) override {
    if (this->parent_->control_buzzer(state))
      this->publish_state(state);
  }
};

}  // namespace esphome::m5_unit_ncir2
