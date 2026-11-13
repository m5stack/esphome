#pragma once

#include "esphome/components/light/addressable_light.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "../m5_unit_8servos2_chain.h"

namespace esphome::m5_unit_8servos2_chain {

class M5Unit8Servos2Light : public light::AddressableLight, public Parented<M5Unit8Servos2Chain> {
 public:
  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void set_num_leds(uint8_t num_leds) { this->num_leds_ = num_leds; }
  /// First entry of this channel in the unit's global RGB buffer; assigned from the channel layout.
  void set_offset(uint8_t offset) { this->offset_ = offset; }

  void setup() override;
  void dump_config() override;
  void write_state(light::LightState *state) override;

  int32_t size() const override { return this->num_leds_; }
  light::LightTraits get_traits() override;
  void clear_effect_data() override;
  /// The light state restores its colour at HARDWARE - 1, so the pixel buffer must exist by then.
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  light::ESPColorView get_view_internal(int32_t index) const override;

  uint8_t *buffer_{nullptr};
  uint8_t *effect_data_{nullptr};
  uint8_t channel_{0};
  uint8_t num_leds_{1};
  uint8_t offset_{0};
};

}  // namespace esphome::m5_unit_8servos2_chain
