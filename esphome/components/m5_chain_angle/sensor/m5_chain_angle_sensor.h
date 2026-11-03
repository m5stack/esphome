#pragma once

#include "../m5_chain_angle.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::m5_chain_angle {

class M5ChainAngleSensor : public sensor::Sensor, public Parented<M5ChainAngle> {};

}  // namespace esphome::m5_chain_angle
