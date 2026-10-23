#include "si5351.h"

#include <cmath>

#include "esphome/core/log.h"


namespace esphome::si5351 {

static const char *const TAG = "si5351";

// Register 3 uses inverted polarity: 1 means the output is disabled.
static const uint8_t CLOCK_POWER_DOWN = 0x80;
static const uint8_t CLOCK_INTEGER_MODE = 0x40;

void Si5351Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Si5351...");

  if (this->oeb_pin_ != nullptr) {
    this->oeb_pin_->setup();
    // OEB is active low: drive it high so the outputs stay off while we program.
    this->oeb_pin_->digital_write(true);
  }
  if (this->ssen_pin_ != nullptr) {
    this->ssen_pin_->setup();
    // SSEN enables spread spectrum when high.
    this->ssen_pin_->digital_write(true);
  }

  if (this->plan_ == nullptr || this->plan_count_ == 0) {
    ESP_LOGE(TAG, "No register plan was generated");
    this->mark_failed();
    return;
  }

  if (!this->write_plan_()) {
    this->mark_failed();
    return;
  }

  if (this->oeb_pin_ != nullptr) {
    this->oeb_pin_->digital_write(false);
  }

  ESP_LOGI(TAG, "Si5351 initialised @0x%02X, %u register(s) written", this->address_,
           this->plan_count_);
}

uint8_t Si5351Component::plan_value_(uint8_t reg, uint8_t fallback) const {
  for (uint16_t i = 0; i < this->plan_count_; i++) {
    if (this->plan_[2 * i] == reg) {
      return this->plan_[2 * i + 1];
    }
  }
  return fallback;
}

bool Si5351Component::write_plan_() {
  // Follows the I2C programming procedure of AN619 section 6.1.
  if (!this->write_byte(SI5351_REG_OUTPUT_ENABLE, 0xFF)) {
    ESP_LOGE(TAG, "Failed to disable the outputs");
    return false;
  }
  for (uint8_t index = 0; index < SI5351_CLOCK_COUNT; index++) {
    if (!this->write_byte(SI5351_REG_CLOCK_CONTROL + index, CLOCK_POWER_DOWN)) {
      ESP_LOGE(TAG, "Failed to power down output driver CLK%u", index);
      return false;
    }
  }

  for (uint16_t i = 0; i < this->plan_count_; i++) {
    const uint8_t reg = this->plan_[2 * i];
    if (reg == SI5351_REG_OUTPUT_ENABLE) {
      // Applied last, once everything else is in place.
      continue;
    }
    if (!this->write_byte(reg, this->plan_[2 * i + 1])) {
      ESP_LOGE(TAG, "Failed to write register %u", reg);
      return false;
    }
  }

  const uint8_t enable = this->plan_value_(SI5351_REG_OUTPUT_ENABLE, 0xFF);
  if (!this->write_byte(SI5351_REG_OUTPUT_ENABLE, enable)) {
    ESP_LOGE(TAG, "Failed to enable the outputs");
    return false;
  }
  this->output_mask_ = enable;
  return true;
}

bool Si5351Component::write_multisynth_(uint8_t index, uint8_t r_code, bool divby4, int32_t p1,
                                        int32_t p2, int32_t p3) {
  uint8_t buffer[8];
  buffer[0] = (p3 >> 8) & 0xFF;
  buffer[1] = p3 & 0xFF;
  buffer[2] = ((r_code & 0x07) << 4) | (divby4 ? (0b11 << 2) : 0) | ((p1 >> 16) & 0x03);
  buffer[3] = (p1 >> 8) & 0xFF;
  buffer[4] = p1 & 0xFF;
  buffer[5] = ((p3 >> 16) & 0x0F) << 4 | ((p2 >> 16) & 0x0F);
  buffer[6] = (p2 >> 8) & 0xFF;
  buffer[7] = p2 & 0xFF;
  return this->write_bytes(SI5351_REG_MULTISYNTH_BASE + index * 8, buffer, 8);
}

bool Si5351Component::set_clock_frequency(uint8_t index, double frequency) {
  if (index >= SI5351_CLOCK_COUNT) {
    ESP_LOGE(TAG, "CLK%u does not exist", index);
    return false;
  }
  auto &clock = this->clocks_[index];
  if (!clock.configured) {
    ESP_LOGE(TAG, "CLK%u is not configured", index);
    return false;
  }
  if (!clock.synthesised) {
    ESP_LOGE(TAG, "CLK%u bypasses the multisynth and cannot change frequency", index);
    return false;
  }
  if (frequency <= 0.0) {
    ESP_LOGE(TAG, "CLK%u: invalid frequency %f", index, frequency);
    return false;
  }

  if (index >= 6) {
    // Multisynth 6/7 only divide by even integers between 6 and 254.
    const auto divider = static_cast<int32_t>(std::lround(clock.vco / frequency));
    if (divider < 6 || divider > 254 || (divider % 2) != 0) {
      ESP_LOGE(TAG, "CLK%u: %f Hz needs divider %d, CLK6/CLK7 only allow even integers 6..254",
               index, frequency, divider);
      return false;
    }
    if (!this->write_byte(index == 6 ? SI5351_REG_MS6_P1 : SI5351_REG_MS7_P1,
                          static_cast<uint8_t>(divider))) {
      ESP_LOGE(TAG, "CLK%u: failed to write the divider", index);
      return false;
    }
    clock.frequency = static_cast<uint32_t>(std::lround(frequency));
    return true;
  }

  double divider = static_cast<double>(clock.vco) / frequency;
  uint8_t r_code = 0;
  while (divider > 2048.0 && r_code < 7) {
    divider /= 2.0;
    r_code++;
  }
  if (divider > 2048.0 || divider < 4.0) {
    ESP_LOGE(TAG, "CLK%u: %f Hz is outside the reachable range (divider %f)", index, frequency,
             divider);
    return false;
  }

  const double nearest = std::round(divider);
  if (std::fabs(divider - nearest) <= 1e-6 * nearest) {
    divider = nearest;
  }
  const bool divby4 = divider == 4.0;
  if (divider < 8.0 && !divby4 && divider != 6.0) {
    ESP_LOGE(TAG, "CLK%u: %f Hz needs divider %f, which is not a valid multisynth ratio", index,
             frequency, divider);
    return false;
  }

  auto a = static_cast<int32_t>(std::floor(divider));
  auto b = static_cast<int32_t>(std::lround((divider - a) * SI5351_MULTISYNTH_DENOM));
  int32_t c = SI5351_MULTISYNTH_DENOM;
  if (b >= c) {
    a++;
    b = 0;
  }
  bool integer_mode = false;
  if (b == 0 && (a % 2) == 0) {
    // Integer mode is only valid for even integer dividers and improves jitter.
    integer_mode = true;
    c = 1;
  }

  const int32_t floor_term = (128 * b) / c;
  const int32_t p1 = 128 * a + floor_term - 512;
  const int32_t p2 = 128 * b - c * floor_term;
  if (!this->write_multisynth_(index, r_code, divby4, p1, p2, c)) {
    ESP_LOGE(TAG, "CLK%u: failed to write the multisynth registers", index);
    return false;
  }

  uint8_t control = 0;
  if (!this->read_byte(SI5351_REG_CLOCK_CONTROL + index, &control)) {
    ESP_LOGE(TAG, "CLK%u: failed to read the control register", index);
    return false;
  }
  if (integer_mode) {
    control |= CLOCK_INTEGER_MODE;
  } else {
    control &= ~CLOCK_INTEGER_MODE;
  }
  if (!this->write_byte(SI5351_REG_CLOCK_CONTROL + index, control)) {
    ESP_LOGE(TAG, "CLK%u: failed to update the control register", index);
    return false;
  }

  clock.frequency = static_cast<uint32_t>(std::lround(frequency));
  return true;
}

void Si5351Component::set_clock_enabled(uint8_t index, bool enabled) {
  if (index >= SI5351_CLOCK_COUNT) {
    return;
  }
  const uint8_t mask = 1 << index;
  uint8_t value = this->output_mask_;
  if (enabled) {
    value &= ~mask;
  } else {
    value |= mask;
  }
  if (value == this->output_mask_) {
    return;
  }
  if (!this->write_byte(SI5351_REG_OUTPUT_ENABLE, value)) {
    ESP_LOGE(TAG, "CLK%u: failed to %s the output", index, enabled ? "enable" : "disable");
    return;
  }
  this->output_mask_ = value;
}

void Si5351Component::set_spread_spectrum_enabled(bool enabled) {
  if (this->ssen_pin_ == nullptr) {
    ESP_LOGW(TAG, "No SSEN pin configured, spread spectrum cannot be switched at runtime");
    return;
  }
  this->ssen_pin_->digital_write(enabled);
}

bool Si5351Component::read_status(uint8_t *value) {
  return this->read_byte(SI5351_REG_STATUS, value);
}

void Si5351Component::dump_config() {
  ESP_LOGCONFIG(TAG, "Si5351 clock generator:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Communication failed");
    return;
  }
  ESP_LOGCONFIG(TAG, "  OEB pin: %s", this->oeb_pin_ != nullptr ? "configured" : "not used");
  ESP_LOGCONFIG(TAG, "  SSEN pin: %s", this->ssen_pin_ != nullptr ? "configured" : "not used");
  for (uint8_t index = 0; index < SI5351_CLOCK_COUNT; index++) {
    const auto &clock = this->clocks_[index];
    if (!clock.configured) {
      continue;
    }
    ESP_LOGCONFIG(TAG, "  CLK%u: %.6f MHz%s%s", index, clock.frequency / 1e6,
                  clock.synthesised ? "" : " (bypass)",
                  this->is_clock_enabled(index) ? "" : " [disabled]");
  }
}

} // namespace esphome::si5351

