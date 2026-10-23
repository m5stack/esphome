/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::aw87559 {

static const uint8_t AW87559_REG_CHIP_ID = 0x00;
static const uint8_t AW87559_REG_SYSCTRL = 0x01;
static const uint8_t AW87559_REG_PAGR = 0x06;

static const uint8_t AW87559_CHIP_ID = 0x5A;

// SYSCTRL (0x01) bits
static const uint8_t AW87559_SYSCTRL_EN_SW = 0x40;     // chip software enable
static const uint8_t AW87559_SYSCTRL_EN_BOOST = 0x10;  // boost converter enable
static const uint8_t AW87559_SYSCTRL_EN_PA = 0x08;     // class-D PA enable

static const uint8_t AW87559_PA_GAIN_MASK = 0x1F;

class AW87559Component : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void on_shutdown() override;

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_pa_gain_code(uint8_t pa_gain_code) { this->pa_gain_code_ = pa_gain_code; }
  void set_startup_delay(uint32_t startup_delay) { this->startup_delay_ = startup_delay; }
  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }

  /// @brief Enables or disables the power amplifier (chip, boost and class-D stage).
  /// @return True if the amplifier state was written successfully.
  bool set_pa_enabled(bool enabled);
  bool is_pa_enabled() const { return this->pa_enabled_; }

  void add_on_pa_state_callback(std::function<void(bool)> &&callback) {
    this->pa_state_callback_.add(std::move(callback));
  }

 protected:
  GPIOPin *reset_pin_{nullptr};
  uint8_t pa_gain_code_{16};  // 16 -> 24dB, the datasheet default
  uint32_t startup_delay_{1000};
  uint8_t sysctrl_{0x38};  // SYSCTRL power-on default, kept in sync after setup
  bool pa_enabled_{false};
  CallbackManager<void(bool)> pa_state_callback_;
};

}  // namespace esphome::aw87559
