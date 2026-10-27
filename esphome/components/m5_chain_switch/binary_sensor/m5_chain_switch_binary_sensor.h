#pragma once

#include "../m5_chain_switch.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome::m5_chain_switch {

class M5ChainSwitchBinarySensor : public binary_sensor::BinarySensor, public Parented<M5ChainSwitch> {};

}  // namespace esphome::m5_chain_switch
