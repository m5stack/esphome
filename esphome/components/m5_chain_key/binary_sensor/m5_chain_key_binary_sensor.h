#pragma once

#include "../m5_chain_key.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome::m5_chain_key {

class M5ChainKeyBinarySensor : public binary_sensor::BinarySensor, public Parented<M5ChainKey> {};

}  // namespace esphome::m5_chain_key
