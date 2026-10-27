#pragma once

#include "../m5_chain_switch.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::m5_chain_switch {

class M5ChainSwitchSensor : public sensor::Sensor, public Parented<M5ChainSwitch> {};

}  // namespace esphome::m5_chain_switch
