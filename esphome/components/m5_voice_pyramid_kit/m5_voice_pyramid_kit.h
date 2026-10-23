#pragma once

#include <cstdint>
#include <cstdio>
#include <utility>

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"

namespace esphome::m5_voice_pyramid_kit {

/// M5Stack Echo Pyramid STM32 controller (I2C protocol V2).
static constexpr uint8_t REG_TOUCH_STATUS = 0x00;
static constexpr uint8_t REG_CHANNEL1_BRIGHTNESS = 0x10;
static constexpr uint8_t REG_CHANNEL2_BRIGHTNESS = 0x11;
static constexpr uint8_t REG_CHANNEL1_RGB = 0x20;
static constexpr uint8_t REG_CHANNEL2_RGB = 0x60;
static constexpr uint8_t REG_SPK_RESET = 0xA0;
static constexpr uint8_t REG_USB_VOLTAGE = 0xB0;
static constexpr uint8_t REG_SAVE_CONFIG = 0xF0;

static constexpr uint8_t NUM_LEDS_PER_CHANNEL = 14;
static constexpr uint8_t NUM_TOUCH_BUTTONS = 4;
// The STM32 firmware stores one LED as B, G, R, 0x00 (4-byte stride) even though
// only the first three bytes carry color. The register window is 14 * 4 = 56 bytes.
static constexpr uint8_t BYTES_PER_LED = 4;

/// Swipe event codes published by the swipe_event sensor (same values as the legacy pyramidtouch component).
enum SwipeEventCode : uint8_t {
  SWIPE_LEFT_UP = 1,     // Touch1 -> Touch2
  SWIPE_LEFT_DOWN = 2,   // Touch2 -> Touch1
  SWIPE_RIGHT_UP = 3,    // Touch4 -> Touch3
  SWIPE_RIGHT_DOWN = 4,  // Touch3 -> Touch4
};

class M5VoicePyramidKit : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::IO; }

  void set_channel_brightness(uint8_t channel, uint8_t brightness);
  void set_swipe_timeout(uint32_t timeout_ms) { this->swipe_timeout_ms_ = timeout_ms; }
  void set_touch_polling_enabled(bool enabled);
  void set_voltage_update_interval(uint32_t interval_ms) { this->voltage_update_interval_ms_ = interval_ms; }

  /// Initialize a channel's hardware brightness (percent, 0-100). Called from setup().
  void set_initial_brightness(uint8_t channel, uint8_t brightness);

  /// Write a contiguous B, G, R block for one channel. The 3-byte-per-LED input
  /// is expanded to the firmware's 4-byte (B, G, R, 0x00) register layout.
  bool write_channel_rgb(uint8_t channel, uint8_t start_led, const uint8_t *data, size_t len, bool reverse = false);

  /// Trigger the SPK reset pulse (register 0xA0).
  bool reset_spk();

  /// Persist the current channel brightness to the controller flash (register 0xF0).
  bool save_channel_brightness(uint8_t channel);

  /// Read the USB 5V rail in millivolts (register 0xB0/0xB1).
  bool read_usb_voltage(float *voltage_mv);

  template<typename F> void add_touch_callback(F &&callback) { this->touch_callbacks_.add(std::forward<F>(callback)); }
  template<typename F> void add_swipe_callback(F &&callback) { this->swipe_callbacks_.add(std::forward<F>(callback)); }
  template<typename F> void add_voltage_callback(F &&callback) {
    this->voltage_callbacks_.add(std::forward<F>(callback));
  }

 protected:
  void poll_touch_(uint32_t now);
  void detect_swipe_(uint32_t now, uint8_t mask);
  void handle_swipe_press_(uint32_t now, bool left, uint8_t touch);

  CallbackManager<void(uint8_t)> touch_callbacks_;
  CallbackManager<void(uint8_t)> swipe_callbacks_;
  CallbackManager<void(float)> voltage_callbacks_;

  uint8_t channel1_brightness_{100};
  uint8_t channel2_brightness_{100};
  bool touch_polling_enabled_{true};
  bool touch_state_valid_{false};
  uint8_t last_touch_mask_{0};
  uint32_t swipe_timeout_ms_{500};
  uint32_t voltage_update_interval_ms_{5000};
  uint32_t last_voltage_ms_{0};

  // Swipe detection state, 0 when no gesture is in progress.
  uint8_t left_swipe_first_{0};
  uint8_t right_swipe_first_{0};
  uint32_t left_swipe_time_ms_{0};
  uint32_t right_swipe_time_ms_{0};

  // Reused for the register + RGB burst so LED updates do not allocate.
  uint8_t tx_buffer_[1 + NUM_LEDS_PER_CHANNEL * BYTES_PER_LED];
};

/// Virtual GPIO exposed by the kit so the AW87559 reset can be driven through register 0xA0.
class M5VoicePyramidKitPin : public GPIOPin, public Parented<M5VoicePyramidKit> {
 public:
  void setup() override;
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  gpio::Flags get_flags() const override { return this->flags_; }
  size_t dump_summary(char *buffer, size_t len) const override;

  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { this->flags_ = flags; }

 protected:
  uint8_t pin_{0};
  bool inverted_{false};
  gpio::Flags flags_{gpio::FLAG_OUTPUT};
};

template<typename... Ts> class ResetSpkAction : public Action<Ts...>, public Parented<M5VoicePyramidKit> {
 public:
  using Parented<M5VoicePyramidKit>::Parented;
  void play(const Ts &...x) override { this->parent_->reset_spk(); }
};

template<typename... Ts> class SetBrightnessAction : public Action<Ts...>, public Parented<M5VoicePyramidKit> {
 public:
  using Parented<M5VoicePyramidKit>::Parented;
  TEMPLATABLE_VALUE(uint8_t, brightness)
  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void play(const Ts &...x) override {
    this->parent_->set_channel_brightness(this->channel_, this->brightness_.value(x...));
  }

 protected:
  uint8_t channel_{1};
};

template<typename... Ts> class SaveBrightnessAction : public Action<Ts...>, public Parented<M5VoicePyramidKit> {
 public:
  using Parented<M5VoicePyramidKit>::Parented;
  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void play(const Ts &...x) override { this->parent_->save_channel_brightness(this->channel_); }

 protected:
  uint8_t channel_{1};
};

template<typename... Ts> class SetTouchPollingAction : public Action<Ts...>, public Parented<M5VoicePyramidKit> {
 public:
  using Parented<M5VoicePyramidKit>::Parented;
  TEMPLATABLE_VALUE(bool, enabled)
  void play(const Ts &...x) override { this->parent_->set_touch_polling_enabled(this->enabled_.value(x...)); }
};

}  // namespace esphome::m5_voice_pyramid_kit
