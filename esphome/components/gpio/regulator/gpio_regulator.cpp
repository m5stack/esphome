#include "gpio_regulator.h"

#include "esphome/core/log.h"

namespace esphome::gpio {

ESPHOME_LOG_TAG(TAG, "gpio.regulator");

bool GPIORegulator::init_regulator_() {
  if (this->pin_ == nullptr) {
    this->mark_failed(LOG_STR("No pin configured"));
    return false;
  }
  this->pin_->setup();
  if (!this->is_boot_on()) {
    this->pin_->digital_write(false);
    this->enabled_ = false;
  }
  return true;
}

bool GPIORegulator::enable_hw_() {
  if (this->pin_ == nullptr)
    return false;
  this->pin_->digital_write(true);
  this->enabled_ = true;
  return true;
}

bool GPIORegulator::disable_hw_() {
  if (this->pin_ == nullptr)
    return false;
  this->pin_->digital_write(false);
  this->enabled_ = false;
  return true;
}

float GPIORegulator::get_setup_priority() const {
  // A pin on the MCU can be driven before the buses. A pin behind an expander
  // has to wait for that expander to be set up at IO priority.
  if (this->pin_ != nullptr && this->pin_->is_internal() && this->is_boot_on())
    return setup_priority::POWER;
  return setup_priority::IO;
}

void GPIORegulator::dump_config() {
  regulator::Regulator::dump_config();
  LOG_PIN("  Pin: ", this->pin_);
  ESP_LOGCONFIG(TAG, "  Enabled: %s", YESNO(this->enabled_));
}

}  // namespace esphome::gpio
