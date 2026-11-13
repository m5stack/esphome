#include "axp2101_sensor.h"

#include <cmath>

#include "esphome/core/log.h"

namespace esphome::axp2101 {

ESPHOME_LOG_TAG(TAG, "axp2101.sensor");

void AXP2101Sensor::publish_from_chip() {
  float value = NAN;
  if (!this->read_value_(value)) {
    return;
  }
  this->publish_state(value);
}

void AXP2101Sensor::dump_config() {
  // The value is refreshed by the hub, so there is no interval to report here.
  LOG_SENSOR("  ", "Sensor", this);
}

void AXP2101AdcSensor::setup() {
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
  // REG 30H powers every channel down by default except battery voltage and
  // TS, so reading a channel without switching it on returns zero.
  if (!this->parent_->set_adc_enabled(*this->channel_, true)) {
    ESP_LOGE(TAG, "Could not enable the ADC channel for %s", this->channel_name_);
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
  }
}

bool AXP2101AdcSensor::read_value_(float &value) {
  if (this->channel_ == nullptr) {
    return false;
  }
  value = this->parent_->read_adc(*this->channel_);
  return !std::isnan(value);
}

bool AXP2101SocSensor::read_value_(float &value) {
  uint8_t percent = 0;
  if (!this->parent_->read_state_of_charge(percent)) {
    return false;
  }
  value = percent;
  return true;
}

}  // namespace esphome::axp2101
