#include "m5_voice_pyramid_kit.h"

#include <cinttypes>
#include <cstring>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::m5_voice_pyramid_kit {

static const char *const TAG = "m5_voice_pyramid_kit";

void M5VoicePyramidKit::set_initial_brightness(uint8_t channel, uint8_t brightness) {
  if (channel == 1) {
    this->channel1_brightness_ = brightness;
  } else if (channel == 2) {
    this->channel2_brightness_ = brightness;
  }
}

void M5VoicePyramidKit::setup() {
  ESP_LOGCONFIG(TAG, "Initializing M5Stack Voice Pyramid Kit");

  this->set_channel_brightness(1, this->channel1_brightness_);
  this->set_channel_brightness(2, this->channel2_brightness_);

  this->last_voltage_ms_ = millis() - this->voltage_update_interval_ms_;
}

void M5VoicePyramidKit::dump_config() {
  ESP_LOGCONFIG(TAG, "M5Stack Voice Pyramid Kit:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Touch polling: %s", ONOFF(this->touch_polling_enabled_));
  ESP_LOGCONFIG(TAG, "  Swipe timeout: %" PRIu32 " ms", this->swipe_timeout_ms_);
  ESP_LOGCONFIG(TAG, "  Channel 1 brightness: %u%%", this->channel1_brightness_);
  ESP_LOGCONFIG(TAG, "  Channel 2 brightness: %u%%", this->channel2_brightness_);
  ESP_LOGCONFIG(TAG, "  USB voltage update interval: %" PRIu32 " ms", this->voltage_update_interval_ms_);
}

void M5VoicePyramidKit::update() {
  const uint32_t now = millis();

  if (this->touch_polling_enabled_ && (this->touch_callbacks_.size() != 0 || this->swipe_callbacks_.size() != 0)) {
    this->poll_touch_(now);
  }

  if (this->voltage_callbacks_.size() != 0 && now - this->last_voltage_ms_ >= this->voltage_update_interval_ms_) {
    float voltage_mv = 0.0f;
    if (this->read_usb_voltage(&voltage_mv)) {
      this->voltage_callbacks_.call(voltage_mv);
    }
    this->last_voltage_ms_ = now;
  }
}

void M5VoicePyramidKit::poll_touch_(uint32_t now) {
  uint8_t raw[NUM_TOUCH_BUTTONS] = {0};
  if (this->read_register(REG_TOUCH_STATUS, raw, NUM_TOUCH_BUTTONS) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  this->status_clear_warning();

  uint8_t mask = 0;
  for (uint8_t i = 0; i < NUM_TOUCH_BUTTONS; i++) {
    if ((raw[i] & 0x01) != 0) {
      mask |= (1u << i);
    }
  }

  if (this->touch_state_valid_ && mask == this->last_touch_mask_) {
    return;
  }

  const bool had_state = this->touch_state_valid_;
  this->touch_state_valid_ = true;

  if (this->swipe_callbacks_.size() != 0 && had_state) {
    this->detect_swipe_(now, mask);
  }

  this->last_touch_mask_ = mask;
  this->touch_callbacks_.call(mask);
}

void M5VoicePyramidKit::detect_swipe_(uint32_t now, uint8_t mask) {
  const uint8_t pressed = mask & ~this->last_touch_mask_;

  if ((pressed & 0x01) != 0) {
    this->handle_swipe_press_(now, true, 1);
  }
  if ((pressed & 0x02) != 0) {
    this->handle_swipe_press_(now, true, 2);
  }
  if ((pressed & 0x04) != 0) {
    this->handle_swipe_press_(now, false, 3);
  }
  if ((pressed & 0x08) != 0) {
    this->handle_swipe_press_(now, false, 4);
  }

  if ((mask & 0x03) == 0) {
    this->left_swipe_first_ = 0;
  }
  if ((mask & 0x0C) == 0) {
    this->right_swipe_first_ = 0;
  }
  if (this->left_swipe_first_ != 0 && now - this->left_swipe_time_ms_ > this->swipe_timeout_ms_) {
    this->left_swipe_first_ = 0;
  }
  if (this->right_swipe_first_ != 0 && now - this->right_swipe_time_ms_ > this->swipe_timeout_ms_) {
    this->right_swipe_first_ = 0;
  }
}

void M5VoicePyramidKit::handle_swipe_press_(uint32_t now, bool left, uint8_t touch) {
  uint8_t &first = left ? this->left_swipe_first_ : this->right_swipe_first_;
  uint32_t &first_ms = left ? this->left_swipe_time_ms_ : this->right_swipe_time_ms_;

  if (first == 0 || now - first_ms > this->swipe_timeout_ms_) {
    first = touch;
    first_ms = now;
    return;
  }

  if (left) {
    if (first == 1 && touch == 2) {
      this->swipe_callbacks_.call(SWIPE_LEFT_UP);
      first = 0;
    } else if (first == 2 && touch == 1) {
      this->swipe_callbacks_.call(SWIPE_LEFT_DOWN);
      first = 0;
    }
  } else {
    if (first == 3 && touch == 4) {
      this->swipe_callbacks_.call(SWIPE_RIGHT_DOWN);
      first = 0;
    } else if (first == 4 && touch == 3) {
      this->swipe_callbacks_.call(SWIPE_RIGHT_UP);
      first = 0;
    }
  }
}

void M5VoicePyramidKit::set_touch_polling_enabled(bool enabled) {
  if (this->touch_polling_enabled_ == enabled) {
    return;
  }
  this->touch_polling_enabled_ = enabled;
  if (enabled) {
    // Force a fresh publish (and a fresh swipe baseline) after being suspended.
    this->touch_state_valid_ = false;
    this->left_swipe_first_ = 0;
    this->right_swipe_first_ = 0;
  }
}

void M5VoicePyramidKit::set_channel_brightness(uint8_t channel, uint8_t brightness) {
  if (channel < 1 || channel > 2) {
    return;
  }
  if (brightness > 100) {
    brightness = 100;
  }

  if (channel == 1) {
    this->channel1_brightness_ = brightness;
  } else {
    this->channel2_brightness_ = brightness;
  }

  const uint8_t reg = (channel == 1) ? REG_CHANNEL1_BRIGHTNESS : REG_CHANNEL2_BRIGHTNESS;
  if (this->write_register(reg, &brightness, 1) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Failed to set channel %u brightness", channel);
    this->status_set_warning();
    return;
  }
  this->status_clear_warning();
}

bool M5VoicePyramidKit::save_channel_brightness(uint8_t channel) {
  if (channel < 1 || channel > 2) {
    return false;
  }
  const uint8_t value = channel;
  if (this->write_register(REG_SAVE_CONFIG, &value, 1) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Failed to save channel %u brightness to flash", channel);
    this->status_set_warning();
    return false;
  }
  // The controller needs about 20 ms to erase the flash page; it will not acknowledge other writes while busy.
  delay(20);
  return true;
}

bool M5VoicePyramidKit::write_channel_rgb(uint8_t channel, uint8_t start_led, const uint8_t *data, size_t len,
                                          bool reverse) {
  if (channel < 1 || channel > 2 || data == nullptr || (len % 3) != 0) {
    return false;
  }
  const size_t led_count = len / 3;
  const size_t tx_len = 1 + led_count * BYTES_PER_LED;
  if (start_led + led_count > NUM_LEDS_PER_CHANNEL || tx_len > sizeof(this->tx_buffer_)) {
    return false;
  }

  this->tx_buffer_[0] =
      static_cast<uint8_t>((channel == 1 ? REG_CHANNEL1_RGB : REG_CHANNEL2_RGB) + start_led * BYTES_PER_LED);
  for (size_t i = 0; i < led_count; i++) {
    const size_t src = (reverse ? (led_count - 1 - i) : i) * 3;
    uint8_t *dst = this->tx_buffer_ + 1 + i * BYTES_PER_LED;
    dst[0] = data[src + 0];
    dst[1] = data[src + 1];
    dst[2] = data[src + 2];
    dst[3] = 0x00;
  }
  return this->write(this->tx_buffer_, tx_len) == i2c::ERROR_OK;
}

bool M5VoicePyramidKit::reset_spk() {
  const uint8_t value = 1;
  if (this->write_register(REG_SPK_RESET, &value, 1) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Failed to reset the speaker amplifier");
    this->status_set_warning();
    return false;
  }
  return true;
}

bool M5VoicePyramidKit::read_usb_voltage(float *voltage_mv) {
  if (voltage_mv == nullptr) {
    return false;
  }
  uint8_t raw[2] = {0};
  if (this->read_register(REG_USB_VOLTAGE, raw, 2) != i2c::ERROR_OK) {
    this->status_set_warning();
    return false;
  }
  this->status_clear_warning();
  *voltage_mv = static_cast<float>(static_cast<uint16_t>(raw[1] << 8) | raw[0]);
  return true;
}

void M5VoicePyramidKitPin::setup() { this->pin_mode(this->flags_); }

void M5VoicePyramidKitPin::pin_mode(gpio::Flags flags) { this->flags_ = flags; }

bool M5VoicePyramidKitPin::digital_read() { return false; }

void M5VoicePyramidKitPin::digital_write(bool value) {
  // Register 0xA0 is a reset trigger, not a level: a high write pulses SPK_RST.
  if ((value != this->inverted_) && this->parent_ != nullptr) {
    this->parent_->reset_spk();
  }
}

size_t M5VoicePyramidKitPin::dump_summary(char *buffer, size_t len) const {
  return snprintf(buffer, len, "SPK reset via m5_voice_pyramid_kit");
}

}  // namespace esphome::m5_voice_pyramid_kit
