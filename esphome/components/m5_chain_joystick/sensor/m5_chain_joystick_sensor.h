#pragma once

#include "esphome/components/m5_chain_joystick/m5_chain_joystick.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::m5_chain_joystick {

class M5ChainJoystickSensor : public sensor::Sensor, public Parented<M5ChainJoystick> {};

}  // namespace esphome::m5_chain_joystick
