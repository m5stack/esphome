#pragma once

#include "../m5_chain_mic.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::m5_chain_mic {

class M5ChainMicSensor : public sensor::Sensor, public Parented<M5ChainMic> {};

}  // namespace esphome::m5_chain_mic
