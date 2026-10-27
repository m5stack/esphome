#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

#include "../m5_chain_tof.h"

namespace esphome::m5_chain_tof {

class M5ChainToFSensor : public sensor::Sensor, public Parented<M5ChainToF> {};

}  // namespace esphome::m5_chain_tof
