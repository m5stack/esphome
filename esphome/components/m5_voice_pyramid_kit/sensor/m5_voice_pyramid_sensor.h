#pragma once

#include <cstdint>

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "../m5_voice_pyramid_kit.h"

namespace esphome::m5_voice_pyramid_kit {

enum M5VoicePyramidKitSensorType : uint8_t {
  SWIPE_EVENT = 0,
  USB_VOLTAGE = 1,
};

class M5VoicePyramidKitSensor : public sensor::Sensor, public Component, public Parented<M5VoicePyramidKit> {
 public:
  void setup() override;
  void dump_config() override;

  void set_type(M5VoicePyramidKitSensorType type) { this->type_ = type; }

 protected:
  M5VoicePyramidKitSensorType type_{SWIPE_EVENT};
};

}  // namespace esphome::m5_voice_pyramid_kit
