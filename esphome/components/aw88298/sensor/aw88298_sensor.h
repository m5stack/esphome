#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/defines.h"

namespace esphome::aw88298 {

/// One of the amplifier's measurements. The hub polls the registers and publishes here, so the
/// entity itself has no state and no setup of its own.
class AW88298Sensor : public sensor::Sensor {};

}  // namespace esphome::aw88298
