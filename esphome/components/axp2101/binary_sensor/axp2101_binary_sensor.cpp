#include "axp2101_binary_sensor.h"

#include "esphome/core/log.h"

namespace esphome::axp2101 {

ESPHOME_LOG_TAG(TAG, "axp2101.binary_sensor");

void AXP2101StatusBinarySensor::setup() {
  if (this->flag_name_ == nullptr) {
    this->mark_failed(LOG_STR("No status flag configured"));
    return;
  }
  this->flag_ = this->parent_->find_status_flag(this->flag_name_);
  if (this->flag_ == nullptr) {
    ESP_LOGE(TAG, "'%s' is not a status flag of this chip", this->flag_name_);
    this->mark_failed(LOG_STR("Unknown status flag"));
  }
}

void AXP2101StatusBinarySensor::dump_config() { LOG_BINARY_SENSOR("  ", "Status", this); }

void AXP2101StatusBinarySensor::publish_from_chip() {
  if (this->flag_ == nullptr || !this->parent_->has_status()) {
    // With no status read there is nothing to report; publishing false would
    // claim the flag is clear when the chip was not reached at all.
    return;
  }
  this->publish_state(this->parent_->status_bit(this->flag_->bit));
}

}  // namespace esphome::axp2101
