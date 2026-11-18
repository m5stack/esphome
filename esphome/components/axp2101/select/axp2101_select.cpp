#include "axp2101_select.h"

#include "esphome/core/log.h"

namespace esphome::axp2101 {

ESPHOME_LOG_TAG(TAG, "axp2101.select");

void AXP2101Select::setup() {
  if (this->select_name_ == nullptr) {
    this->mark_failed(LOG_STR("No setting configured"));
    return;
  }
  this->select_ = this->parent_->find_select(this->select_name_);
  if (this->select_ == nullptr) {
    ESP_LOGE(TAG, "'%s' is not a setting of this chip", this->select_name_);
    this->mark_failed(LOG_STR("Unknown setting"));
    return;
  }
  if (this->select_->field.count != this->size()) {
    // The option list and the value table come from the same config key, so a
    // mismatch is a code generation bug rather than a runtime condition.
    ESP_LOGE(TAG, "%s has %u options but %u values", this->select_name_, static_cast<unsigned>(this->size()),
             static_cast<unsigned>(this->select_->field.count));
    this->mark_failed(LOG_STR("Option count does not match the value table"));
    return;
  }

  if (this->initial_option_index_ >= 0) {
    const auto target = static_cast<uint8_t>(this->initial_option_index_);
    if (target >= this->select_->field.count) {
      ESP_LOGE(TAG, "'%s' has no option %u", this->select_name_, static_cast<unsigned>(target));
      this->mark_failed(LOG_STR("Initial option out of range"));
      return;
    }
    uint8_t code = 0;
    uint8_t current = 0;
    // Only write when the chip holds something else, so a reboot neither rewrites
    // the register nor logs a change.
    if (!this->read_current_code_(code) || !this->index_for_code_(code, current) || current != target) {
      this->control(target);
      return;
    }
    this->publish_state(target);
    return;
  }

  this->publish_current_option_();
}

bool AXP2101Select::read_current_code_(uint8_t &code) { return this->parent_->read_select_code(*this->select_, code); }

bool AXP2101Select::index_for_code_(uint8_t code, uint8_t &index) const {
  for (uint8_t i = 0; i < this->select_->field.count; i++) {
    if (this->select_->field.entries[i].code == code) {
      index = i;
      return true;
    }
  }
  return false;
}

void AXP2101Select::publish_current_option_() {
  uint8_t code = 0;
  if (!this->read_current_code_(code)) {
    return;
  }
  uint8_t index = 0;
  if (this->index_for_code_(code, index)) {
    this->publish_state(index);
    return;
  }
  // A code outside the table is reserved by the datasheet, so there is no
  // option to show and the last known one stays.
  ESP_LOGW(TAG, "%s reads back reserved code %u", this->select_name_, static_cast<unsigned>(code));
}

void AXP2101Select::control(size_t index) {
  if (this->select_ == nullptr) {
    return;
  }
  if (!this->parent_->write_select_index(*this->select_, index)) {
    ESP_LOGE(TAG, "Could not set %s", this->select_name_);
    // Report what the chip holds rather than what was asked for.
    this->publish_current_option_();
    return;
  }
  this->publish_state(index);
}

void AXP2101Select::dump_config() {
  LOG_SELECT("  ", "AXP2101 Select", this);
  if (this->select_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Setting: %s", this->select_name_);
  }
}

}  // namespace esphome::axp2101
