#pragma once

#include "esphome/components/m5_chain_dlight/m5_chain_dlight.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/helpers.h"

namespace esphome::m5_chain_dlight {

class M5ChainDLightSensor : public sensor::Sensor, public Parented<M5ChainDLight> {};

}  // namespace esphome::m5_chain_dlight
