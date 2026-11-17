#pragma once

#include "esphome/components/m5_chain_imu/m5_chain_imu.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/helpers.h"

namespace esphome::m5_chain_imu {

class M5ChainImuSensor : public sensor::Sensor, public Parented<M5ChainImu> {};

}  // namespace esphome::m5_chain_imu
