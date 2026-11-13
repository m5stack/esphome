#include "axp2101_switch.h"

#include "esphome/core/log.h"

namespace esphome::axp2101 {

ESPHOME_LOG_TAG(TAG, "axp2101.switch");

void AXP2101Switch::setup() {
  if (this->rail_name_ == nullptr) {
    this->mark_failed(LOG_STR("No rail configured"));
    return;
  }
  this->rail_ = this->parent_->find_rail(this->rail_name_);
  if (this->rail_ == nullptr) {
    ESP_LOGE(TAG, "'%s' is not a power rail of this chip", this->rail_name_);
    this->mark_failed(LOG_STR("Unknown rail"));
    return;
  }
  if (!axp_base::axp_rail_has_enable(*this->rail_)) {
    ESP_LOGE(TAG, "%s cannot be switched", this->rail_->name);
    this->mark_failed(LOG_STR("Rail cannot be switched"));
    return;
  }

  this->control(this->get_initial_state_with_restore_mode().value_or(false));
}

void AXP2101Switch::write_state(bool state) {
  if (this->rail_ == nullptr) {
    return;
  }
  if (!this->parent_->set_rail_enabled(this->rail_, state)) {
    // The bus is gone, so the rail's real state is unknown. Publishing a guess
    // would be worse than leaving the last known state in place.
    ESP_LOGE(TAG, "Could not %s %s", state ? LOG_STR_LITERAL("enable") : LOG_STR_LITERAL("disable"), this->rail_->name);
    return;
  }
  this->publish_state(state);
}

void AXP2101Switch::dump_config() {
  LOG_SWITCH("  ", "AXP2101 Switch", this);
  if (this->rail_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Rail: %s (%u mA max)", this->rail_->name, static_cast<unsigned>(this->rail_->max_ma));
  }
}

}  // namespace esphome::axp2101
