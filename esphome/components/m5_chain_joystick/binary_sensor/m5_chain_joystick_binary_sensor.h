#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/m5_chain_joystick/m5_chain_joystick.h"

namespace esphome::m5_chain_joystick {

class M5ChainJoystickBinarySensor : public binary_sensor::BinarySensor, public Parented<M5ChainJoystick> {};

}  // namespace esphome::m5_chain_joystick
