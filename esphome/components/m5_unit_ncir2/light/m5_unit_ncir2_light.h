#pragma once

#include <cmath>

#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/m5_unit_ncir2/m5_unit_ncir2.h"
#include "esphome/core/helpers.h"

namespace esphome::m5_unit_ncir2 {

class M5UnitNCIR2Light : public light::LightOutput, public Parented<M5UnitNCIR2> {
 public:
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  }

  void write_state(light::LightState *state) override {
    float red, green, blue;
    state->current_values_as_rgb(&red, &green, &blue);
    this->parent_->write_led(to_byte_(red), to_byte_(green), to_byte_(blue));
  }

 protected:
  static uint8_t to_byte_(float value) { return static_cast<uint8_t>(lroundf(value * 255.0f)); }
};

}  // namespace esphome::m5_unit_ncir2
