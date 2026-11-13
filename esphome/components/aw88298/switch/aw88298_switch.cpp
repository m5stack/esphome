#include "aw88298_switch.h"

#include "esphome/core/log.h"

namespace esphome::aw88298 {

ESPHOME_LOG_TAG(TAG, "aw88298.switch");

void AW88298Switch::setup() {
  if (auto initial = this->get_initial_state_with_restore_mode()) {
    this->write_state(*initial);
    return;
  }
  this->publish_state(this->parent_->get_setting(this->setting_) > 0.0f);
}

void AW88298Switch::write_state(bool state) {
  if (!this->parent_->set_setting(this->setting_, state ? 1.0f : 0.0f)) {
    ESP_LOGE(TAG, "Could not %s %s", state ? LOG_STR_LITERAL("enable") : LOG_STR_LITERAL("disable"),
             LOG_STR_ARG(setting_name(this->setting_)));
    // The write failed, so what the amplifier holds now is unknown; keep the last known state.
    return;
  }
  this->publish_state(state);
}

void AW88298Switch::dump_config() {
  LOG_SWITCH("  ", "AW88298 Switch", this);
  ESP_LOGCONFIG(TAG, "  Setting: %s", LOG_STR_ARG(setting_name(this->setting_)));
}

}  // namespace esphome::aw88298
