#pragma once

#include "esphome/components/m5_chain_env/m5_chain_env.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/helpers.h"

namespace esphome::m5_chain_env {

class M5ChainEnvSensor : public sensor::Sensor, public Parented<M5ChainEnv> {};

}  // namespace esphome::m5_chain_env
