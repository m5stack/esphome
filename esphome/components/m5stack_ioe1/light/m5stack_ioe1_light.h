#pragma once

#include <array>
#include <cstdint>

#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/channel_colors.h"
#include "esphome/components/light/light_output.h"

#include "../m5stack_ioe1.h"

namespace esphome::m5stack_ioe1 {

class M5StackIOE1AddressableLight : public light::AddressableLight, public Parented<M5StackIOE1Component> {
 public:
  void setup() override;
  void write_state(light::LightState *state) override;

  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void set_num_leds(uint8_t num_leds) { this->num_leds_ = num_leds; }

  int32_t size() const override { return this->num_leds_; }

  light::LightTraits get_traits() override {
    light::LightTraits traits;
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  }

  void clear_effect_data() override { this->internal_accessor_.clear_effect_data(this->num_leds_); }

 protected:
  light::CorrectedBufferColorAccessor internal_accessor_{nullptr, nullptr, {0, 1, 2}, &this->correction_};
  uint8_t num_leds_{0};
  // The on-device driver only supports RGB565 (16 bits), but the light buffer
  // stays RGB888 for compatibility and is converted when written.
  std::array<uint8_t, M5StackIOE1Component::MAX_LEDS * 2> rgb565_buf_{};
};

}  // namespace esphome::m5stack_ioe1
