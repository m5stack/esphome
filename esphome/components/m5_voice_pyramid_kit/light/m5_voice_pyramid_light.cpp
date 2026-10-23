#include "m5_voice_pyramid_light.h"

#include <cinttypes>
#include <cstring>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::m5_voice_pyramid_kit {

static const char *const TAG = "m5_voice_pyramid_kit.light";

void M5VoicePyramidKitLight::setup() {
  const size_t buffer_size = static_cast<size_t>(this->num_leds_) * 3;
  RAMAllocator<uint8_t> allocator;

  this->buf_ = allocator.allocate(buffer_size);
  this->last_buf_ = allocator.allocate(buffer_size);
  this->effect_data_ = allocator.allocate(this->num_leds_);
  if (this->buf_ == nullptr || this->last_buf_ == nullptr || this->effect_data_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate the LED buffers");
    this->mark_failed();
    return;
  }

  std::memset(this->buf_, 0, buffer_size);
  std::memset(this->last_buf_, 0, buffer_size);
  std::memset(this->effect_data_, 0, this->num_leds_);
}

void M5VoicePyramidKitLight::dump_config() {
  ESP_LOGCONFIG(TAG, "M5Stack Voice Pyramid Kit Light:");
  ESP_LOGCONFIG(TAG, "  Channel: %u", this->channel_);
  ESP_LOGCONFIG(TAG, "  LEDs: %" PRId32 " (start %u)", this->num_leds_, this->start_led_);
  ESP_LOGCONFIG(TAG, "  Reverse: %s", YESNO(this->reverse_));
  ESP_LOGCONFIG(TAG, "  Refresh interval: %" PRIu32 " us", this->refresh_interval_us_);
}

light::LightTraits M5VoicePyramidKitLight::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::RGB});
  return traits;
}

void M5VoicePyramidKitLight::write_state(light::LightState *state) {
  const size_t len = static_cast<size_t>(this->num_leds_) * 3;
  const uint32_t now = micros();

  if (this->refresh_interval_us_ != 0 && this->last_refresh_us_ != 0 &&
      now - this->last_refresh_us_ < this->refresh_interval_us_) {
    this->schedule_show();
    return;
  }
  this->last_refresh_us_ = now;

  if (this->has_written_ && std::memcmp(this->buf_, this->last_buf_, len) == 0) {
    this->mark_shown_();
    return;
  }

  if (!this->parent_->write_channel_rgb(this->channel_, this->start_led_, this->buf_, len, this->reverse_)) {
    this->status_set_warning();
    this->schedule_show();
    return;
  }

  this->status_clear_warning();
  std::memcpy(this->last_buf_, this->buf_, len);
  this->has_written_ = true;
  this->mark_shown_();
}

void M5VoicePyramidKitLight::clear_effect_data() { std::memset(this->effect_data_, 0, this->num_leds_); }

light::ESPColorView M5VoicePyramidKitLight::get_view_internal(int32_t index) const {
  const size_t pos = static_cast<size_t>(index) * 3;
  uint8_t *led = this->buf_ + pos;
  return {
      led + 2,  // red
      led + 1,  // green
      led + 0,  // blue
      nullptr, this->effect_data_ + index, &this->correction_,
  };
}

}  // namespace esphome::m5_voice_pyramid_kit
