#include "m5_chain_mono_light.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "esphome/core/log.h"

namespace esphome::m5_chain_mono {

ESPHOME_LOG_TAG(TAG, "m5_chain_mono.light");

void M5ChainMonoLight::setup() {
  RAMAllocator<uint8_t> allocator;
  this->buffer_ = allocator.allocate(64 * 3);
  this->effect_data_ = allocator.allocate(64);
  if (this->buffer_ == nullptr || this->effect_data_ == nullptr) {
    this->mark_failed();
    return;
  }
  std::memset(this->buffer_, 0, 64 * 3);
  std::memset(this->effect_data_, 0, 64);
}

void M5ChainMonoLight::dump_config() {
  ESP_LOGCONFIG(TAG, "M5Stack Chain Mono Addressable Light: 64 pixels (8x8), 8 screen brightness levels");
}

light::LightTraits M5ChainMonoLight::get_traits() {
  // The panel has one bit per pixel: it can only dim, not colour, so brightness is the only mode.
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
  return traits;
}

uint8_t M5ChainMonoLight::screen_brightness_level_(float level) {
  if (level <= 0.0f)
    return 0;
  // Any level above zero is kept lit, so a very dim light does not turn the panel off.
  const long step = lroundf(level * MAX_SCREEN_BRIGHTNESS_LEVEL);
  return static_cast<uint8_t>(std::clamp<long>(step, 1, MAX_SCREEN_BRIGHTNESS_LEVEL));
}

void M5ChainMonoLight::write_state(light::LightState *state) {
  const auto values = state->current_values;
  uint8_t display_buffer[8]{};
  for (uint8_t index = 0; index < 64; index++) {
    const uint8_t *pixel = this->buffer_ + index * 3;
    if (pixel[0] != 0 || pixel[1] != 0 || pixel[2] != 0)
      display_buffer[index / 8] |= static_cast<uint8_t>(0x80U >> (index % 8));
  }
  // The level goes first so the new frame is not shown at the previous brightness.
  const float level = values.get_state() * values.get_brightness();
  auto status = this->parent_->set_screen_brightness(screen_brightness_level_(level));
  if (status == m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
    status = this->parent_->set_display_buffer(display_buffer, sizeof(display_buffer));
  if (status == m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
    this->status_clear_warning();
  else
    this->status_set_warning();
}

void M5ChainMonoLight::clear_effect_data() { std::memset(this->effect_data_, 0, 64); }

light::ESPColorView M5ChainMonoLight::get_view_internal(int32_t index) const {
  auto *pixel = this->buffer_ + index * 3;
  return {pixel, pixel + 1, pixel + 2, nullptr, this->effect_data_ + index, &this->correction_};
}

}  // namespace esphome::m5_chain_mono
