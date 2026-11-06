#include "m5_chain_rgb_light.h"

#include <cstring>

#include "esphome/core/log.h"

namespace esphome::m5_chain_rgb {

ESPHOME_LOG_TAG(TAG, "m5_chain_rgb.light");

void M5ChainRGBLight::setup() {
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

void M5ChainRGBLight::dump_config() {
  ESP_LOGCONFIG(TAG, "M5Stack Chain RGB Addressable Light: 64 pixels (8x8), module brightness pinned to %u%%",
                MAX_SCREEN_BRIGHTNESS);
}

light::LightTraits M5ChainRGBLight::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::RGB});
  return traits;
}

void M5ChainRGBLight::write_state(light::LightState *) {
  // The module multiplies every frame by its own brightness, which it keeps in flash: pin it to the
  // top of its range so the panel always reaches the same maximum, whatever a previous tool saved
  // there. Dimming stays in the pixel buffer, where the gamma curve is applied.
  auto status = this->parent_->set_screen_brightness(MAX_SCREEN_BRIGHTNESS);
  if (status != m5_chain_base::ChainStatus::CHAIN_STATUS_OK) {
    this->status_set_warning();
    return;
  }
  uint8_t display_buffer[128]{};
  for (uint8_t index = 0; index < 64; index++) {
    const uint8_t *pixel = this->buffer_ + index * 3;
    const uint16_t rgb565 =
        static_cast<uint16_t>(((pixel[0] & 0xF8) << 8) | ((pixel[1] & 0xFC) << 3) | (pixel[2] >> 3));
    display_buffer[index * 2] = static_cast<uint8_t>(rgb565);
    display_buffer[index * 2 + 1] = static_cast<uint8_t>(rgb565 >> 8);
  }
  if (this->parent_->set_display_buffer(display_buffer, sizeof(display_buffer)) ==
      m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
    this->status_clear_warning();
  else
    this->status_set_warning();
}

void M5ChainRGBLight::clear_effect_data() { std::memset(this->effect_data_, 0, 64); }

light::ESPColorView M5ChainRGBLight::get_view_internal(int32_t index) const {
  auto *pixel = this->buffer_ + index * 3;
  return {pixel, pixel + 1, pixel + 2, nullptr, this->effect_data_ + index, &this->correction_};
}

}  // namespace esphome::m5_chain_rgb
