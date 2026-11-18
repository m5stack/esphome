#include "axp192_sensor.h"

#include <cmath>

#include "esphome/core/log.h"

namespace esphome::axp192 {

ESPHOME_LOG_TAG(TAG, "axp192.sensor");

void AXP192Sensor::publish_from_chip() {
  float value = NAN;
  if (!this->read_value_(value)) {
    return;
  }
  this->publish_state(value);
}

void AXP192Sensor::dump_config() {
  // The value is refreshed by the hub, so there is no interval to report here.
  LOG_SENSOR("  ", "Sensor", this);
}

void AXP192AdcSensor::setup() {
  if (this->channel_name_ == nullptr) {
    this->mark_failed(LOG_STR("No ADC channel configured"));
    return;
  }
  this->channel_ = this->parent_->find_adc_channel(this->channel_name_);
  if (this->channel_ == nullptr) {
    ESP_LOGE(TAG, "'%s' is not an ADC channel of this chip", this->channel_name_);
    this->mark_failed(LOG_STR("Unknown ADC channel"));
    return;
  }
  // REG 82H and REG 83H leave most channels off, and a channel that is off
  // reads back as zero rather than as a stale value.
  if (!this->parent_->set_adc_enabled(*this->channel_, true)) {
    ESP_LOGE(TAG, "Could not enable the ADC channel for %s", this->channel_name_);
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
  }
}

bool AXP192AdcSensor::read_value_(float &value) {
  if (this->channel_ == nullptr) {
    return false;
  }
  value = this->parent_->read_adc(*this->channel_);
  return !std::isnan(value);
}

}  // namespace esphome::axp192
