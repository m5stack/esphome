#pragma once

#include "esphome/components/light/addressable_light.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "../m5_voice_pyramid_kit.h"

namespace esphome::m5_voice_pyramid_kit {

class M5VoicePyramidKitLight : public light::AddressableLight, public Parented<M5VoicePyramidKit> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  light::LightTraits get_traits() override;
  void write_state(light::LightState *state) override;
  int32_t size() const override { return this->num_leds_; }
  void clear_effect_data() override;

  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void set_start_led(uint8_t start_led) { this->start_led_ = start_led; }
  void set_num_leds(int32_t num_leds) { this->num_leds_ = num_leds; }
  void set_reverse(bool reverse) { this->reverse_ = reverse; }
  void set_refresh_interval(uint32_t refresh_interval_us) { this->refresh_interval_us_ = refresh_interval_us; }

 protected:
  light::ESPColorView get_view_internal(int32_t index) const override;

  // Stored as B, G, R to match the controller's register order.
  uint8_t *buf_{nullptr};
  uint8_t *last_buf_{nullptr};
  uint8_t *effect_data_{nullptr};
  int32_t num_leds_{NUM_LEDS_PER_CHANNEL};
  uint8_t channel_{1};
  uint8_t start_led_{0};
  uint32_t refresh_interval_us_{33000};
  uint32_t last_refresh_us_{0};
  bool has_written_{false};
  bool reverse_{false};
};

}  // namespace esphome::m5_voice_pyramid_kit
