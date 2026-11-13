#include "m5_unit_8servos2_chain_light.h"

#include <cstring>

#include "esphome/core/log.h"

namespace esphome::m5_unit_8servos2_chain {

ESPHOME_LOG_TAG(TAG, "m5_unit_8servos2_chain.light");

void M5Unit8Servos2Light::setup() {
  this->parent_->request_mode(this->channel_, Servos2Mode::SERVOS2_MODE_RGB);

  RAMAllocator<uint8_t> allocator;
  this->buffer_ = allocator.allocate(this->num_leds_ * 3);
  this->effect_data_ = allocator.allocate(this->num_leds_);
  if (this->buffer_ == nullptr || this->effect_data_ == nullptr) {
    this->mark_failed();
    return;
  }
  std::memset(this->buffer_, 0, this->num_leds_ * 3);
  std::memset(this->effect_data_, 0, this->num_leds_);
}

void M5Unit8Servos2Light::dump_config() {
  ESP_LOGCONFIG(TAG, "M5Stack Unit 8Servos2 Chain Addressable Light:");
  ESP_LOGCONFIG(TAG, "  Channel: %u", this->channel_);
  ESP_LOGCONFIG(TAG, "  LEDs: %u", this->num_leds_);
  ESP_LOGCONFIG(TAG, "  Buffer offset: %u", this->offset_);
}

void M5Unit8Servos2Light::write_state(light::LightState *) {
  if (this->parent_ == nullptr || this->buffer_ == nullptr)
    return;
  this->parent_->request_mode(this->channel_, Servos2Mode::SERVOS2_MODE_RGB);
  this->parent_->set_rgb_buffer(this->channel_, this->offset_, this->num_leds_, this->buffer_);
}

light::LightTraits M5Unit8Servos2Light::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::RGB});
  return traits;
}

void M5Unit8Servos2Light::clear_effect_data() { std::memset(this->effect_data_, 0, this->num_leds_); }

light::ESPColorView M5Unit8Servos2Light::get_view_internal(int32_t index) const {
  auto *pixel = this->buffer_ + index * 3;
  return {pixel, pixel + 1, pixel + 2, nullptr, this->effect_data_ + index, &this->correction_};
}

}  // namespace esphome::m5_unit_8servos2_chain
