#pragma once

#include "esphome/components/m5_chain_encoder/m5_chain_encoder.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::m5_chain_encoder {

class M5ChainEncoderSensor : public sensor::Sensor, public Parented<M5ChainEncoder> {};

}  // namespace esphome::m5_chain_encoder
