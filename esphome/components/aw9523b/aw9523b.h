// Copyright (c) M5Stack. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
// datasheet: https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/products/core/CoreS3/AW9523B-EN.pdf
#pragma once

#include <cstring>

#include "esphome/components/gpio_expander/cached_gpio.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"

namespace esphome::aw9523b {

enum AW9523BP0DriveMode : uint8_t { OPEN_DRAIN = 0x00, PUSH_PULL = 0x01 };

enum AW9523BLEDMaxCurrent : uint8_t {
  CURRENT_MAX = 0x00,
  CURRENT_3QUARTERS = 0x01,
  CURRENT_HALF = 0x02,
  CURRENT_1QUARTER = 0x03
};

class AW9523BComponent : public Component,
                         public i2c::I2CDevice,
                         public gpio_expander::CachedGpioExpander<uint8_t, 16> {
 public:
  AW9523BComponent() = default;

  void setup() override;
  void loop() override;
  void dump_config() override;

  /// Configure a pin as input or output. Input pins have their interrupt enabled
  /// when the hub has an interrupt pin configured.
  void pin_mode(uint8_t pin, gpio::Flags flags);

  float get_setup_priority() const override { return setup_priority::IO; }

  /// Indicate if the component should reset the state during setup
  void set_reset(bool reset) { this->reset_ = reset; }

  /// Pin connected to the chip's open-drain, active-low INTN output
  void set_interrupt_pin(InternalGPIOPin *interrupt_pin) { this->interrupt_pin_ = interrupt_pin; }

  /// Set P0 output mode: 1 for push-pull, 0 for open-drain
  void set_p0_drive_mode(AW9523BP0DriveMode mode) { this->p0_drive_mode_ = mode; }

  /// Set global max current for LED mode
  void set_led_max_current(AW9523BLEDMaxCurrent current) { this->led_max_current_ = current; }

  /// Switch a pin from GPIO mode to LED (PWM) mode; called by the output platform
  bool setup_led_mode(uint8_t pin);

  /// Set the LED (PWM) brightness of a pin
  void write_led_current(uint8_t pin, uint8_t current);

 protected:
  static void IRAM_ATTR gpio_intr(AW9523BComponent *arg);

  bool digital_read_hw(uint8_t pin) override;
  bool digital_read_cache(uint8_t pin) override;
  void digital_write_hw(uint8_t pin, bool value) override;

  /// Mark every bank as cached, for a read that refreshed all of them at once.
  void set_pin_cache_valid_() { memset(this->read_cache_valid_, 0xFF, CACHE_SIZE_BYTES); }

  /// Read Input_Port0 and Input_Port1. Two separate transactions are required:
  /// the chip only releases INTN once both registers have been read.
  bool read_inputs_();
  bool read_gpio_modes_();
  bool write_gpio_modes_();
  bool read_gpio_outputs_();
  bool read_gpio_interrupts_();
  bool write_gpio_interrupts_();
  bool read_led_modes_();
  bool write_led_modes_();
  /// Enable or disable the interrupt of a single pin, writing only on change
  bool set_gpio_interrupt_(uint8_t pin, bool enabled);

  /// 1 = input, 0 = output (Config_Port0/1, 04H/05H)
  uint16_t mode_mask_{0x0000};
  /// 1 = HIGH, 0 = LOW (Output_Port0/1, 02H/03H)
  uint16_t output_mask_{0x0000};
  /// Last Input_Port0/1 read - 1 = HIGH, 0 = LOW
  uint16_t input_mask_{0x0000};
  /// 1 = interrupt disabled, 0 = enabled (Int_Port0/1, 06H/07H).
  /// The chip powers up with every interrupt enabled (0000H).
  uint16_t interrupt_mask_{0xFFFF};
  /// 1 = GPIO mode, 0 = LED mode (LED_Mode_Switch, 12H/13H).
  /// The chip powers up in GPIO mode (FFFFH).
  uint16_t led_mode_mask_{0xFFFF};
  /// P0 output drive mode - 0: open-drain, 1: push-pull
  AW9523BP0DriveMode p0_drive_mode_{OPEN_DRAIN};
  /// Chip reset flag
  bool reset_{true};
  /// Global Imax for LED control, typical Imax is 37 mA
  /// Register value | Max current    | Configure range
  /// 0              | Imax * 1       | [0 ~ Imax]
  /// 1              | Imax * 0.75    | [0 ~ Imax * 0.75]
  /// 2              | Imax * 0.5     | [0 ~ Imax * 0.5]
  /// 3              | Imax * 0.25    | [0 ~ Imax * 0.25]
  AW9523BLEDMaxCurrent led_max_current_{CURRENT_MAX};

  /// Interrupt pin
  InternalGPIOPin *interrupt_pin_{nullptr};
};

/// Pin of the AW9523B hub. Kept local rather than using a shared expander pin class so the component
/// builds against ESPHome releases that do not have one.
class AW9523BGPIOPin final : public GPIOPin, public Parented<AW9523BComponent> {
 public:
  void setup() override { this->pin_mode(this->flags_); }
  void pin_mode(gpio::Flags flags) override { this->parent_->pin_mode(this->pin_, flags); }
  bool digital_read() override { return this->parent_->digital_read(this->pin_) != this->inverted_; }
  void digital_write(bool value) override { this->parent_->digital_write(this->pin_, value != this->inverted_); }
  size_t dump_summary(char *buffer, size_t len) const override {
    return buf_append_printf(buffer, len, 0, "%u via AW9523B", this->pin_);
  }

  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { this->flags_ = flags; }

  gpio::Flags get_flags() const override { return this->flags_; }

 protected:
  uint8_t pin_{0};
  bool inverted_{false};
  gpio::Flags flags_{};
};

}  // namespace esphome::aw9523b
