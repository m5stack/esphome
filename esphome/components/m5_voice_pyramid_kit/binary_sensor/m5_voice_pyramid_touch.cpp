#include "m5_voice_pyramid_touch.h"

#include "esphome/core/log.h"

namespace esphome::m5_voice_pyramid_kit {

static const char *const TAG = "m5_voice_pyramid_kit.touch";

void M5VoicePyramidKitTouch::setup() {
  this->parent_->add_touch_callback(
      [this](uint8_t mask) { this->publish_state((mask & (1u << (this->touch_ - 1))) != 0); });
  this->publish_initial_state(false);
}

void M5VoicePyramidKitTouch::dump_config() {
  ESP_LOGCONFIG(TAG, "M5Stack Voice Pyramid Kit Touch %u", this->touch_);
  LOG_BINARY_SENSOR("  ", "Touch", this);
}

}  // namespace esphome::m5_voice_pyramid_kit
