#pragma once

#include "../m5_chain_mic.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome::m5_chain_mic {

class M5ChainMicBinarySensor : public binary_sensor::BinarySensor, public Parented<M5ChainMic> {};

}  // namespace esphome::m5_chain_mic
