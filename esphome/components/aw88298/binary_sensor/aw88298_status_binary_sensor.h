#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/defines.h"

namespace esphome::aw88298 {

/// One bit of the amplifier's status register. The hub polls the register and publishes here, so
/// the entity itself has no state and no setup of its own.
class AW88298StatusBinarySensor : public binary_sensor::BinarySensor {};

}  // namespace esphome::aw88298
