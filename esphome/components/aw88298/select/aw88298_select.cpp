#include "aw88298_select.h"

#include "esphome/core/log.h"

namespace esphome::aw88298 {

ESPHOME_LOG_TAG(TAG, "aw88298.select");

void AW88298Select::setup() {
  const auto index = static_cast<size_t>(this->parent_->get_setting(this->setting_));
  if (index >= this->size()) {
    ESP_LOGW(TAG, "%s reads back option %u", LOG_STR_ARG(setting_name(this->setting_)), static_cast<unsigned>(index));
    return;
  }
  this->publish_state(index);
}

void AW88298Select::control(size_t index) {
  if (!this->parent_->set_setting(this->setting_, static_cast<float>(index))) {
    ESP_LOGE(TAG, "Could not set %s to option %u", LOG_STR_ARG(setting_name(this->setting_)),
             static_cast<unsigned>(index));
    // The write failed, so report what the amplifier holds rather than what was asked for.
    this->publish_state(static_cast<size_t>(this->parent_->get_setting(this->setting_)));
    return;
  }
  this->publish_state(index);
}

void AW88298Select::dump_config() {
  LOG_SELECT("  ", "AW88298 Select", this);
  ESP_LOGCONFIG(TAG, "  Setting: %s", LOG_STR_ARG(setting_name(this->setting_)));
}

}  // namespace esphome::aw88298
