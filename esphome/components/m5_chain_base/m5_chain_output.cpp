#include "m5_chain_output.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome::m5_chain_base {

ESPHOME_LOG_TAG(TAG, "m5_chain_base.output");

void M5ChainLedOutput::write_state(float state) {
  if (this->parent_ == nullptr) {
    ESP_LOGW(TAG, "Parent is null");
    return;
  }

  state = std::clamp(state, 0.0f, 1.0f);
  switch (this->channel_) {
    case LedChannel::LED_BRIGHTNESS:
    case LedChannel::RGB_BRIGHTNESS: {
      const auto status = this->parent_->set_led_brightness(static_cast<uint8_t>(state * 100.0f));
      if (status != ChainStatus::CHAIN_STATUS_OK) {
        ESP_LOGW(TAG, "Failed to set RGB brightness (status=0x%02X)", static_cast<uint8_t>(status));
      }
      break;
    }
    case LedChannel::RGB_RED:
    case LedChannel::RGB_GREEN:
    case LedChannel::RGB_BLUE: {
      const auto component = static_cast<uint8_t>(this->channel_) - static_cast<uint8_t>(LedChannel::RGB_RED);
      const auto status = this->parent_->set_rgb_color_component(component, static_cast<uint8_t>(state * 255.0f));
      if (status != ChainStatus::CHAIN_STATUS_OK) {
        ESP_LOGW(TAG, "Failed to set RGB color (status=0x%02X)", static_cast<uint8_t>(status));
      }
      break;
    }
  }
}

}  // namespace esphome::m5_chain_base
