#pragma once

#include "esphome/components/m5_chain_imu/m5_chain_imu.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/helpers.h"

namespace esphome::m5_chain_imu {

class M5ChainImuBinarySensor : public binary_sensor::BinarySensor, public Parented<M5ChainImu> {};

}  // namespace esphome::m5_chain_imu
