// Copyright (c) M5Stack. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
// datasheet: https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1210/IO_Expander_Datasheet_EN.pdf
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "esphome/components/gpio_expander/cached_gpio_component.h"
#include "esphome/components/gpio_expander/expander_gpio_pin.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/hal.h"

namespace esphome::m5stack_ioe1 {

enum M5StackIOE1ADCChannel : uint8_t {
  ADC_NONE = 0,
  ADC_1 = 1,
  ADC_2 = 2,
  ADC_3 = 3,
  ADC_4 = 4,
  REF_VOLT = 5,
};

enum M5StackIOE1PWMChannel : uint8_t {
  PWM_1 = 0,
  PWM_2 = 1,
  PWM_3 = 2,
  PWM_4 = 3,
};

enum M5StackIOE1PWMPolarity : uint8_t {
  ACTIVE_HIGH = 0,
  ACTIVE_LOW = 1,
};

enum M5StackIOE1InterruptType : uint8_t {
  RISING_EDGE = 1,
  FALLING_EDGE = 0,
};

class M5StackIOE1Component final : public gpio_expander::CachedGpioExpanderComponent<uint8_t, 16>,
                                   public i2c::I2CDevice {
 public:
  static constexpr uint8_t GPIO_COUNT = 14;
  static constexpr uint8_t MAX_LEDS = 32;
  static constexpr uint8_t PWM_CHANNEL_COUNT = 4;
  /// Pin index (IO14) the chip drives the NeoPixel data line on.
  static constexpr uint8_t NEOPIXEL_PIN = 13;

  void setup() override;
  void loop() override;
  void dump_config() override;

  void pin_mode(uint8_t pin, gpio::Flags flags);

  /// Indicate if the component should reset the state during setup
  void set_reset(bool reset) { this->reset_ = reset; }

  /// Store the interrupt setting of one pin; the registers are written during setup().
  void set_pin_interrupt(uint8_t pin, bool enabled, M5StackIOE1InterruptType type);

  /// Set the pwm frequency
  void set_pwm_frequency(uint16_t freq) { this->pwm_freq_ = freq; }

  /// Setup PWM output
  void setup_pwm_channel(M5StackIOE1PWMChannel channel, M5StackIOE1PWMPolarity polar) {
    this->config_pwm_channel_(channel, polar);
  }
  /// Set PWM duty
  void set_pwm_value(M5StackIOE1PWMChannel channel, float duty) { this->write_pwm_duty_(channel, duty); }

  /// Read value from ADC registers
  float get_adc_value(M5StackIOE1ADCChannel channel) { return this->read_adc_(channel); }
  /// Read ADC reference voltage
  float get_adc_ref_voltage() { return this->read_adc_ref_(); }

  /// Read temperature value in degrees Celsius
  float get_temperature_value() { return this->read_temperature_(); }

  /// Setup Neopixel LED: switch IO14 to push-pull and apply the LED count
  void setup_neopixel(uint8_t num);

  /// Set LED color
  void set_led_color(const uint8_t *buf, size_t len) { this->write_led_ram_(buf, len); }

  /// Setup AW8737A
  void setup_aw8737a(uint8_t pin, uint8_t pulse_num) { this->config_aw8737a_(pin, pulse_num); }

  /// Start or stop AW8737A pulse generation
  void start_aw8737a(bool enabled) { this->enable_aw8737a_pulse_(enabled); }

 protected:
  bool digital_read_hw(uint8_t pin) override;
  bool digital_read_cache(uint8_t pin) override;
  void digital_write_hw(uint8_t pin, bool value) override;

  /// Mask for the pin mode - 1 means output, 0 means input
  uint16_t mode_mask_{0};
  /// The mask to write as output state - 1 means HIGH, 0 means LOW
  uint16_t output_mask_{0};
  /// The state read in digital_read_hw - 1 means HIGH, 0 means LOW
  uint16_t input_mask_{0};
  /// The mask to write as pullup state
  uint16_t pull_up_mask_{0};
  /// The mask to write as pulldown state
  uint16_t pull_down_mask_{0};
  /// Mask for drive mode - 1 means open-drain, 0 means push-pull.
  /// Initialized to the chip's open-drain default; configured outputs switch to push-pull.
  uint16_t drv_mode_mask_{0x3FFF};

  /// Pins whose interrupt setting comes from the configuration
  uint16_t interrupt_configured_mask_{0};
  /// Pins with interrupts enabled
  uint16_t interrupt_enable_mask_{0};
  /// Pins configured as rising-edge; cleared bits trigger on falling edge
  uint16_t interrupt_type_mask_{0};
  /// Interrupt status latched by the chip
  uint16_t interrupt_result_mask_{0};

  bool reset_{true};
  /// At least one input has no interrupt, so the loop must poll
  bool polling_needed_{false};

  /// PWM frequency
  uint16_t pwm_freq_{500};
  /// PWM control bits (EN/POL) per channel, preserved when writing the duty cycle
  std::array<uint8_t, PWM_CHANNEL_COUNT> pwm_ctrl_{};

  uint16_t device_uid_{0};
  uint8_t firm_ver_{0};

  bool read_gpio_config_();
  bool write_gpio_modes_();
  bool write_gpio_interrupts_();
  bool read_gpio_interrupt_();
  bool clear_gpio_interrupt_();

  /// Configure PWM channel
  void config_pwm_channel_(M5StackIOE1PWMChannel channel, M5StackIOE1PWMPolarity polar);

  /// Write PWM duty to given channel
  void write_pwm_duty_(M5StackIOE1PWMChannel channel, float duty);

  /// Read value from given ADC channel
  float read_adc_(M5StackIOE1ADCChannel channel);

  /// Read adc reference voltage
  float read_adc_ref_();

  /// Read temperature value
  float read_temperature_();

  /// Configure on device NeoPixel
  void config_neopixel_(uint8_t num);

  /// Write LED RAM data
  void write_led_ram_(const uint8_t *buf, size_t len);

  /// Configure on-device AW8737A pulse
  void config_aw8737a_(uint8_t pin, uint8_t pulse_num);

  /// Enable pulse generation
  void enable_aw8737a_pulse_(bool enabled);
};

using M5StackIOE1GPIOPin = gpio_expander::ExpanderGPIOPin<M5StackIOE1Component>;

}  // namespace esphome::m5stack_ioe1

ESPHOME_EXPANDER_PIN_SUMMARY_DECL(esphome::m5stack_ioe1::M5StackIOE1Component);
