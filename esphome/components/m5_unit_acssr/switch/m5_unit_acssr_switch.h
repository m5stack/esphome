#pragma once

#include "esphome/components/m5_unit_acssr/m5_unit_acssr.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/helpers.h"

namespace esphome::m5_unit_acssr {

class M5UnitACSSRRelaySwitch : public switch_::Switch, public Parented<M5UnitACSSR> {
 protected:
  // The hub publishes the state once the unit has it
  void write_state(bool state) override { this->parent_->write_relay(state); }
};

}  // namespace esphome::m5_unit_acssr
