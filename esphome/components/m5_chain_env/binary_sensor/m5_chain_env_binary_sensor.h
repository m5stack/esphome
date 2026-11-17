#pragma once

#include "esphome/components/m5_chain_env/m5_chain_env.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/helpers.h"

namespace esphome::m5_chain_env {

class M5ChainEnvBinarySensor : public binary_sensor::BinarySensor, public Parented<M5ChainEnv> {};

}  // namespace esphome::m5_chain_env
