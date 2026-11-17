#pragma once

#include "esphome/components/m5_chain_dlight/m5_chain_dlight.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/helpers.h"

namespace esphome::m5_chain_dlight {

class M5ChainDLightBinarySensor : public binary_sensor::BinarySensor, public Parented<M5ChainDLight> {};

}  // namespace esphome::m5_chain_dlight
