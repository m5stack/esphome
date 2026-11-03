#pragma once

#include "esphome/components/light/addressable_light.h"
#include "../m5_chain_rgb.h"

namespace esphome::m5_chain_rgb {

class M5ChainRGBLight : public light::AddressableLight, public Parented<M5ChainRGB> {
 public:
  void setup() override;
  void dump_config() override;
  void write_state(light::LightState *state) override;
  int32_t size() const override { return 64; }
  light::LightTraits get_traits() override;
  void clear_effect_data() override;
  /// The light state restores its colour at HARDWARE - 1, so the pixel buffer must exist by then.
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  light::ESPColorView get_view_internal(int32_t index) const override;
  uint8_t *buffer_{nullptr};
  uint8_t *effect_data_{nullptr};
};

}  // namespace esphome::m5_chain_rgb
