#include "m5_chain_mono_light.h"

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

void M5ChainMonoLight::dump_config() { ESP_LOGCONFIG(TAG, "M5Stack Chain Mono Addressable Light: 64 pixels (8x8)"); }

light::LightTraits M5ChainMonoLight::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::RGB});
  return traits;
}

void M5ChainMonoLight::write_state(light::LightState *) {
  uint8_t display_buffer[8]{};
  for (uint8_t index = 0; index < 64; index++) {
    const uint8_t *pixel = this->buffer_ + index * 3;
    if (pixel[0] != 0 || pixel[1] != 0 || pixel[2] != 0)
      display_buffer[index / 8] |= static_cast<uint8_t>(0x80U >> (index % 8));
  }
  if (this->parent_->set_display_buffer(display_buffer, sizeof(display_buffer)) ==
      m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
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
