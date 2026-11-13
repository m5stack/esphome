#include "axp2101_number.h"

#include <cmath>

#include "esphome/core/log.h"

namespace esphome::axp2101 {

ESPHOME_LOG_TAG(TAG, "axp2101.number");

void AXP2101Number::setup() {
  if (this->number_name_ == nullptr) {
    this->mark_failed(LOG_STR("No setting configured"));
    return;
  }
  this->number_ = this->parent_->find_number(this->number_name_);
  if (this->number_ == nullptr) {
    ESP_LOGE(TAG, "'%s' is not a setting of this chip", this->number_name_);
    this->mark_failed(LOG_STR("Unknown setting"));
    return;
  }
  this->publish_current_value_();
}

void AXP2101Number::publish_current_value_() {
  uint8_t code = 0;
  if (!this->parent_->read_number_code(*this->number_, code)) {
    return;
  }
  if (code < this->number_->min_code || code > this->number_->max_code) {
    // A code outside the range is reserved by the datasheet, so there is no
    // value to show and the last known one stays.
    ESP_LOGW(TAG, "%s reads back reserved code %u", this->number_name_, static_cast<unsigned>(code));
    return;
  }
  this->publish_state(static_cast<float>(axp_base::AxpComponent::decode_number(*this->number_, code)));
}

void AXP2101Number::control(float value) {
  if (this->number_ == nullptr) {
    return;
  }
  const uint8_t code = axp_base::AxpComponent::encode_number(*this->number_, static_cast<int32_t>(lroundf(value)));
  if (!this->parent_->write_number_code(*this->number_, code)) {
    ESP_LOGE(TAG, "Could not set %s", this->number_name_);
    // Report what the chip holds rather than what was asked for.
    this->publish_current_value_();
    return;
  }
  // Publish the decoded code so a value between two steps shows what the chip
  // was actually set to.
  this->publish_state(static_cast<float>(axp_base::AxpComponent::decode_number(*this->number_, code)));
}

void AXP2101Number::dump_config() {
  LOG_NUMBER("  ", "AXP2101 Number", this);
  if (this->number_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Setting: %s", this->number_name_);
  }
}

}  // namespace esphome::axp2101
