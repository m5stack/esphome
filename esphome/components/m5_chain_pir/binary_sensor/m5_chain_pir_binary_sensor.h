#pragma once

#include "../m5_chain_pir.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome::m5_chain_pir {

class M5ChainPirBinarySensor : public binary_sensor::BinarySensor, public Parented<M5ChainPir> {};

}  // namespace esphome::m5_chain_pir
