#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/m5_chain_encoder/m5_chain_encoder.h"

namespace esphome::m5_chain_encoder {

class M5ChainEncoderBinarySensor : public binary_sensor::BinarySensor, public Parented<M5ChainEncoder> {};

}  // namespace esphome::m5_chain_encoder
