#include "axp192_switch.h"

#include "esphome/core/log.h"

namespace esphome::axp192 {

ESPHOME_LOG_TAG(TAG, "axp192.flag_switch");

void AXP192FlagSwitch::setup() {
  if (this->flag_name_ == nullptr) {
    this->mark_failed(LOG_STR("No setting configured"));
    return;
  }
  this->flag_ = this->parent_->find_flag(this->flag_name_);
  if (this->flag_ == nullptr) {
    ESP_LOGE(TAG, "'%s' is not a switchable setting of this chip", this->flag_name_);
    this->mark_failed(LOG_STR("Unknown setting"));
    return;
  }

  // Only a configured restore mode writes at boot; the default leaves whatever
  // the chip decided to do and just reports it.
  if (auto initial = this->get_initial_state_with_restore_mode()) {
    this->control(*initial);
    return;
  }

  bool on = false;
  if (!this->parent_->read_flag(*this->flag_, on)) {
    ESP_LOGW(TAG, "Could not read %s", this->flag_name_);
    return;
  }
  this->publish_state(on);
}

void AXP192FlagSwitch::write_state(bool state) {
  if (this->flag_ == nullptr) {
    return;
  }
  if (!this->parent_->write_flag(*this->flag_, state)) {
    // The bus is gone, so the real state is unknown; keeping the last known one
    // is better than reporting a guess.
    ESP_LOGE(TAG, "Could not %s %s", state ? LOG_STR_LITERAL("enable") : LOG_STR_LITERAL("disable"), this->flag_->name);
    return;
  }
  this->publish_state(state);
}

void AXP192FlagSwitch::dump_config() {
  LOG_SWITCH("  ", "AXP192 Switch", this);
  if (this->flag_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Setting: %s", this->flag_name_);
  }
}

}  // namespace esphome::axp192
