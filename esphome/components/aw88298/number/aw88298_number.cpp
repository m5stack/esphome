#include "aw88298_number.h"

#include "esphome/core/log.h"

namespace esphome::aw88298 {

ESPHOME_LOG_TAG(TAG, "aw88298.number");

void AW88298Number::setup() { this->publish_state(this->parent_->get_setting(this->setting_)); }

void AW88298Number::control(float value) {
  if (!this->parent_->set_setting(this->setting_, value)) {
    ESP_LOGE(TAG, "Could not set %s to %.3f", LOG_STR_ARG(setting_name(this->setting_)), value);
    // The write failed, so report what the amplifier holds rather than what was asked for.
    this->publish_state(this->parent_->get_setting(this->setting_));
    return;
  }
  // The setting keeps the quantised value, so this shows what the register holds.
  this->publish_state(this->parent_->get_setting(this->setting_));
}

void AW88298Number::dump_config() {
  LOG_NUMBER("  ", "AW88298 Number", this);
  ESP_LOGCONFIG(TAG, "  Setting: %s", LOG_STR_ARG(setting_name(this->setting_)));
}

}  // namespace esphome::aw88298
