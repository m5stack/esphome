#pragma once

#include "../m5_chain_switch.h"
#include "esphome/components/switch/switch.h"

namespace esphome::m5_chain_switch {

class M5ChainSwitchEventTriggerSwitch : public switch_::Switch, public Parented<M5ChainSwitch> {
 protected:
  void write_state(bool state) override;
};

}  // namespace esphome::m5_chain_switch
