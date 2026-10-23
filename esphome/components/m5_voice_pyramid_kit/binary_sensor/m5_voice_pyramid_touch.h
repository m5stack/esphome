#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "../m5_voice_pyramid_kit.h"

namespace esphome::m5_voice_pyramid_kit {

class M5VoicePyramidKitTouch : public binary_sensor::BinarySensor,
                               public Component,
                               public Parented<M5VoicePyramidKit> {
 public:
  void setup() override;
  void dump_config() override;

  void set_touch(uint8_t touch) { this->touch_ = touch; }

 protected:
  uint8_t touch_{1};
};

}  // namespace esphome::m5_voice_pyramid_kit
