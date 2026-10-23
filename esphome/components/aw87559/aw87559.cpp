/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "aw87559.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::aw87559 {

static const char *const TAG = "aw87559";

void AW87559Component::setup() {
  if (this->reset_pin_ != nullptr) {
    // RSTN is active low and has an internal pull-down: drive it high to release the reset.
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
  }

  ESP_LOGD(TAG, "Setting up AW87559");

  if (this->startup_delay_ > 0) {
    delay(this->startup_delay_);
  }

  uint8_t chip_id = 0;
  if (!this->read_byte(AW87559_REG_CHIP_ID, &chip_id)) {
    ESP_LOGE(TAG, "Unable to read the chip ID");
    this->mark_failed();
    return;
  }
  if (chip_id != AW87559_CHIP_ID) {
    ESP_LOGW(TAG, "Unexpected chip ID 0x%02X (expected 0x%02X)", chip_id, AW87559_CHIP_ID);
  }

  uint8_t sysctrl = 0;
  if (this->read_byte(AW87559_REG_SYSCTRL, &sysctrl)) {
    this->sysctrl_ = sysctrl;
  }

  if (!this->write_byte(AW87559_REG_PAGR, this->pa_gain_code_ & AW87559_PA_GAIN_MASK)) {
    ESP_LOGE(TAG, "Unable to set the PA gain");
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "AW87559 initialized @0x%02X with %.1fdB PA gain", this->address_, this->pa_gain_code_ * 1.5f);
}

void AW87559Component::dump_config() {
  ESP_LOGCONFIG(TAG, "AW87559 Audio Amplifier:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with AW87559 failed!");
    return;
  }
  ESP_LOGCONFIG(TAG, "  PA Gain: %.1fdB", this->pa_gain_code_ * 1.5f);
  ESP_LOGCONFIG(TAG, "  PA Enabled: %s", ONOFF(this->pa_enabled_));
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
}

void AW87559Component::on_shutdown() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
  }
}

bool AW87559Component::set_pa_enabled(bool enabled) {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Cannot %s the PA, the amplifier failed to set up", enabled ? "enable" : "disable");
    return false;
  }

  uint8_t value = this->sysctrl_;
  const uint8_t pa_bits = AW87559_SYSCTRL_EN_SW | AW87559_SYSCTRL_EN_BOOST | AW87559_SYSCTRL_EN_PA;
  if (enabled) {
    value |= pa_bits;
  } else {
    value &= ~pa_bits;
  }

  if (!this->write_byte(AW87559_REG_SYSCTRL, value)) {
    ESP_LOGE(TAG, "Unable to %s the PA", enabled ? "enable" : "disable");
    return false;
  }
  this->sysctrl_ = value;

  if (this->pa_enabled_ != enabled) {
    this->pa_enabled_ = enabled;
    ESP_LOGD(TAG, "PA %s", enabled ? "enabled" : "disabled");
    this->pa_state_callback_.call(this->pa_enabled_);
  }
  return true;
}

}  // namespace esphome::aw87559
