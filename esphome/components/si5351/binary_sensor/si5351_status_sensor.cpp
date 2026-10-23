#include "si5351_status_sensor.h"

#include "esphome/core/log.h"


namespace esphome::si5351 {

static const char *const TAG = "si5351.binary_sensor";

void Si5351StatusSensor::update() {
  if (this->parent_ == nullptr) {
    this->status_set_warning();
    return;
  }
  uint8_t status = 0;
  if (!this->parent_->read_status(&status)) {
    this->status_set_warning();
    return;
  }
  this->status_clear_warning();
  this->publish_state((status & (1u << this->status_bit_)) != 0);
}

void Si5351StatusSensor::dump_config() {
  LOG_BINARY_SENSOR("", "Si5351 status", this);
  ESP_LOGCONFIG(TAG, "  Status bit: %u", this->status_bit_);
}

} // namespace esphome::si5351

