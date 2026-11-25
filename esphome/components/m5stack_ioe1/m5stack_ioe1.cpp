#include "m5stack_ioe1.h"

#include <algorithm>
#include <cmath>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::m5stack_ioe1 {

ESPHOME_LOG_TAG(TAG, "m5stack_ioe1");

// GPIO registers, each 16-bit register is stored low byte first
static constexpr uint8_t REG_UID_L = 0x00;
static constexpr uint8_t REG_VERSION = 0x02;
static constexpr uint8_t REG_GPIO_M_L = 0x03;
static constexpr uint8_t REG_GPIO_O_L = 0x05;
static constexpr uint8_t REG_GPIO_I_L = 0x07;
static constexpr uint8_t REG_GPIO_PU_L = 0x09;
static constexpr uint8_t REG_GPIO_PD_L = 0x0B;
static constexpr uint8_t REG_GPIO_IE_L = 0x0D;
static constexpr uint8_t REG_GPIO_IP_L = 0x0F;
static constexpr uint8_t REG_GPIO_IS_L = 0x11;
static constexpr uint8_t REG_GPIO_DRV_L = 0x13;

// ADC registers
static constexpr uint8_t REG_ADC_CTRL = 0x15;
static constexpr uint8_t REG_ADC_D_L = 0x16;
static constexpr uint8_t ADC_START = 1 << 6;
static constexpr uint8_t ADC_BUSY = 1 << 7;

// Temperature sensor registers
static constexpr uint8_t REG_TEMP_CTRL = 0x18;
static constexpr uint8_t REG_TEMP_D_L = 0x19;
static constexpr uint8_t TEMP_START = 1 << 6;
static constexpr uint8_t TEMP_BUSY = 1 << 7;

// PWM duty registers, two bytes per channel
static constexpr uint8_t REG_PWM1_DUTY_L = 0x1B;
static constexpr uint8_t PWM_ENABLE = 1 << 7;
static constexpr uint8_t PWM_POLARITY = 1 << 6;

// Configuration registers
static constexpr uint8_t REG_LED_CFG = 0x24;
static constexpr uint8_t REG_PWM_FREQ_L = 0x25;
static constexpr uint8_t REG_REF_VOLT_L = 0x27;
static constexpr uint8_t REG_RESET = 0x29;
static constexpr uint8_t RESET_MAGIC = 0x3A;

// Memory registers
static constexpr uint8_t REG_LED_RAM_START = 0x30;
static constexpr uint8_t REG_AW8737A_PULSE = 0x90;

static constexpr uint8_t LED_NUM_MASK = 0x3F;
static constexpr uint8_t LED_REFRESH = 1 << 6;
static constexpr uint8_t AW8737A_REFRESH = 1 << 7;
static constexpr uint8_t AW8737A_PULSE_MASK = 0x03;

// Keep each I2C write small: the Arduino Wire buffer is limited and ESPHome's
// write path allocates a heap buffer for payloads above 16 bytes.
static constexpr size_t I2C_WRITE_CHUNK_SIZE = 16;
static constexpr uint8_t CONVERSION_POLL_TRIES = 20;

void M5StackIOE1Component::setup() {
  ESP_LOGD(TAG, "Setting up M5STACK_IOE1...");
  if (this->reset_) {
    if (!this->setup_check(this->write_byte(REG_RESET, RESET_MAGIC), LOG_STR("Failed to reset device")))
      return;
    delay(1000);  // NOLINT: the device must finish resetting before setup continues
  } else {
    delay(20);  // NOLINT: let the device finish powering up
  }

  uint8_t uid[2];
  uint8_t revision;
  if (!this->setup_check(this->read_bytes(REG_UID_L, uid, 2) && this->read_byte(REG_VERSION, &revision),
                         LOG_STR(ESP_LOG_MSG_COMM_FAIL)))
    return;

  if (revision == 0x00 || revision == 0xFF) {
    ESP_LOGE(TAG, "Invalid firmware version 0x%02X", revision);
    this->mark_failed(LOG_STR("Invalid firmware version"));
    return;
  }
  this->device_uid_ = (uid[1] << 8) | uid[0];
  this->firm_ver_ = revision;

  // Keep the existing GPIO configuration when the device is not reset
  if (!this->reset_ && !this->read_gpio_config_()) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  if (!this->setup_check(this->write_byte_16_le(REG_PWM_FREQ_L, this->pwm_freq_),
                         LOG_STR("Failed to configure PWM frequency")))
    return;

  if (this->interrupt_pin_ != nullptr) {
    if (!this->setup_check(this->write_gpio_interrupts_(), LOG_STR("Failed to configure GPIO interrupts")))
      return;
    // Drop a stale latched interrupt so attaching the ISR cannot wake immediately
    this->clear_gpio_interrupt_();
  }

  // Attach the INT line last: it also decides whether the loop starts enabled.
  this->setup_interrupt_pin_();
  ESP_LOGD(TAG, "M5STACK_IOE1 setup finished");
}

void M5StackIOE1Component::loop() {
  if (this->is_failed())
    return;
  if (this->interrupt_pin_ != nullptr) {
    // Clearing the latched status releases the INT line
    this->read_gpio_interrupt_();
  }
  this->reset_pin_cache_();
  if (this->interrupt_pin_ != nullptr && !this->polling_needed_) {
    this->disable_loop_when_released_();
  }
}

void M5StackIOE1Component::dump_config() {
  ESP_LOGCONFIG(TAG,
                "M5STACK_IOE1:\n"
                "  Device UID: 0x%04X\n"
                "  Firmware Version: %u\n"
                "  PWM Frequency: %u Hz",
                this->device_uid_, this->firm_ver_, this->pwm_freq_);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_I2C_DEVICE(this)
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}

void M5StackIOE1Component::set_pin_interrupt(uint8_t pin, bool enabled, M5StackIOE1InterruptType type) {
  if (pin >= GPIO_COUNT)
    return;
  const uint16_t mask = 1 << pin;
  this->interrupt_configured_mask_ |= mask;
  assign_bits(this->interrupt_enable_mask_, mask, enabled);
  assign_bits(this->interrupt_type_mask_, mask, type == RISING_EDGE);
}

void M5StackIOE1Component::pin_mode(uint8_t pin, gpio::Flags flags) {
  if (this->is_failed() || pin >= GPIO_COUNT)
    return;

  const uint16_t mask = 1 << pin;
  if (flags & gpio::FLAG_OUTPUT) {
    this->mode_mask_ |= mask;
    assign_bits(this->drv_mode_mask_, mask, (flags & gpio::FLAG_OPEN_DRAIN) != 0);
  } else if (flags & gpio::FLAG_INPUT) {
    this->mode_mask_ &= ~mask;
    assign_bits(this->pull_up_mask_, mask, (flags & gpio::FLAG_PULLUP) != 0);
    assign_bits(this->pull_down_mask_, mask, (flags & gpio::FLAG_PULLDOWN) != 0);
    if ((this->interrupt_enable_mask_ & mask) == 0) {
      // This input has no interrupt, so the loop has to poll it
      this->polling_needed_ = true;
      this->enable_loop();
    }
  }

  this->write_gpio_modes_();
}

bool M5StackIOE1Component::read_gpio_config_() {
  return this->status_comm_result(this->read_byte_16_le(REG_GPIO_M_L, &this->mode_mask_),
                                  LOG_STR("Failed to read GPIO modes")) &&
         this->status_comm_result(this->read_byte_16_le(REG_GPIO_O_L, &this->output_mask_),
                                  LOG_STR("Failed to read GPIO outputs")) &&
         this->status_comm_result(this->read_byte_16_le(REG_GPIO_PU_L, &this->pull_up_mask_),
                                  LOG_STR("Failed to read GPIO pullups")) &&
         this->status_comm_result(this->read_byte_16_le(REG_GPIO_PD_L, &this->pull_down_mask_),
                                  LOG_STR("Failed to read GPIO pulldowns")) &&
         this->status_comm_result(this->read_byte_16_le(REG_GPIO_DRV_L, &this->drv_mode_mask_),
                                  LOG_STR("Failed to read GPIO drive modes"));
}

bool M5StackIOE1Component::write_gpio_modes_() {
  if (this->is_failed())
    return false;

  if (!this->status_comm_result(this->write_byte_16_le(REG_GPIO_M_L, this->mode_mask_),
                                LOG_STR("Failed to write GPIO modes")))
    return false;
  if (!this->status_comm_result(this->write_byte_16_le(REG_GPIO_PU_L, this->pull_up_mask_),
                                LOG_STR("Failed to write GPIO pullups")))
    return false;
  if (!this->status_comm_result(this->write_byte_16_le(REG_GPIO_PD_L, this->pull_down_mask_),
                                LOG_STR("Failed to write GPIO pulldowns")))
    return false;
  if (!this->status_comm_result(this->write_byte_16_le(REG_GPIO_DRV_L, this->drv_mode_mask_),
                                LOG_STR("Failed to write GPIO drive modes")))
    return false;
  return true;
}

bool M5StackIOE1Component::write_gpio_interrupts_() {
  uint16_t enable = this->interrupt_enable_mask_;
  uint16_t type = this->interrupt_type_mask_;

  if (!this->reset_) {
    // Preserve the interrupt settings of pins that are not configured
    uint16_t current_enable;
    uint16_t current_type;
    if (!this->status_comm_result(this->read_byte_16_le(REG_GPIO_IE_L, &current_enable),
                                  LOG_STR("Failed to read GPIO interrupt enables")) ||
        !this->status_comm_result(this->read_byte_16_le(REG_GPIO_IP_L, &current_type),
                                  LOG_STR("Failed to read GPIO interrupt types")))
      return false;
    const uint16_t keep = ~this->interrupt_configured_mask_;
    enable = (current_enable & keep) | this->interrupt_enable_mask_;
    type = (current_type & keep) | this->interrupt_type_mask_;
  }

  if (!this->status_comm_result(this->write_byte_16_le(REG_GPIO_IE_L, enable),
                                LOG_STR("Failed to write GPIO interrupt enables")) ||
      !this->status_comm_result(this->write_byte_16_le(REG_GPIO_IP_L, type),
                                LOG_STR("Failed to write GPIO interrupt types")))
    return false;

  this->interrupt_enable_mask_ = enable;
  this->interrupt_type_mask_ = type;
  return true;
}

bool M5StackIOE1Component::read_gpio_interrupt_() {
  uint16_t status;
  if (!this->status_comm_result(this->read_byte_16_le(REG_GPIO_IS_L, &status),
                                LOG_STR("Failed to read GPIO interrupt status")))
    return false;
  this->interrupt_result_mask_ = status;
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  if (status != 0) {
    ESP_LOGV(TAG, "GPIO interrupt status: 0x%04X", status);
  }
#endif
  return this->clear_gpio_interrupt_();
}

bool M5StackIOE1Component::clear_gpio_interrupt_() {
  // Writing zeros clears the latched flags and lets the open-drain INT line go high
  return this->status_comm_result(this->write_byte_16_le(REG_GPIO_IS_L, 0),
                                  LOG_STR("Failed to clear GPIO interrupt status"));
}

bool M5StackIOE1Component::digital_read_hw(uint8_t pin) {
  if (this->is_failed())
    return false;

  return this->status_comm_result(this->read_byte_16_le(REG_GPIO_I_L, &this->input_mask_),
                                  LOG_STR("Failed to read GPIO state"));
}

bool M5StackIOE1Component::digital_read_cache(uint8_t pin) { return (this->input_mask_ & (1 << pin)) != 0; }

void M5StackIOE1Component::digital_write_hw(uint8_t pin, bool value) {
  if (this->is_failed())
    return;

  assign_bits(this->output_mask_, 1 << pin, value);
  if (!this->status_comm_result(this->write_byte_16_le(REG_GPIO_O_L, this->output_mask_),
                                LOG_STR("Failed to write GPIO output")))
    return;

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  ESP_LOGV(TAG, "Wrote GPIO output: 0b" BYTE_TO_BINARY_PATTERN " " BYTE_TO_BINARY_PATTERN,
           BYTE_TO_BINARY(this->output_mask_ >> 8), BYTE_TO_BINARY(this->output_mask_ & 0xFF));
#endif
}

void M5StackIOE1Component::config_pwm_channel_(M5StackIOE1PWMChannel channel, M5StackIOE1PWMPolarity polar) {
  const uint8_t index = static_cast<uint8_t>(channel);
  if (index >= PWM_CHANNEL_COUNT)
    return;

  uint8_t ctrl = PWM_ENABLE;
  if (polar == ACTIVE_LOW)
    ctrl |= PWM_POLARITY;
  this->pwm_ctrl_[index] = ctrl;

  const uint8_t reg = REG_PWM1_DUTY_L + (index * 2);
  if (!this->status_comm_result(this->write_byte_16_le(reg, static_cast<uint16_t>(ctrl) << 8),
                                LOG_STR("Failed to configure PWM channel")))
    return;
}

void M5StackIOE1Component::write_pwm_duty_(M5StackIOE1PWMChannel channel, float duty) {
  const uint8_t index = static_cast<uint8_t>(channel);
  if (index >= PWM_CHANNEL_COUNT)
    return;

  if (duty <= 0.0f)
    duty = 0.0f;
  if (duty >= 1.0f)
    duty = 1.0f;

  const uint16_t value = static_cast<uint16_t>(duty * 4095.0f + 0.5f) & 0x0FFF;
  // Keep the enable and polarity bits configured by config_pwm_channel_
  const uint16_t data = value | (static_cast<uint16_t>(this->pwm_ctrl_[index]) << 8);

  const uint8_t reg = REG_PWM1_DUTY_L + (index * 2);
  if (!this->status_comm_result(this->write_byte_16_le(reg, data), LOG_STR("Failed to write PWM duty")))
    return;
}

float M5StackIOE1Component::read_adc_(M5StackIOE1ADCChannel channel) {
  if (channel == REF_VOLT)
    return this->get_adc_ref_voltage();

  const uint8_t ctrl = static_cast<uint8_t>(channel) | ADC_START;
  if (!this->status_comm_result(this->write_byte(REG_ADC_CTRL, ctrl), LOG_STR("Failed to start ADC conversion")))
    return NAN;

  uint8_t status = 0;
  for (uint8_t i = 0; i < CONVERSION_POLL_TRIES; i++) {
    delay(1);
    if (!this->status_comm_result(this->read_byte(REG_ADC_CTRL, &status), LOG_STR("Failed to read ADC status")))
      return NAN;
    if ((status & ADC_BUSY) == 0)
      break;
  }
  if (status & ADC_BUSY) {
    ESP_LOGW(TAG, "ADC conversion timed out");
    this->status_set_warning(LOG_STR("ADC conversion timed out"));
    return NAN;
  }

  uint16_t value;
  if (!this->status_comm_result(this->read_byte_16_le(REG_ADC_D_L, &value), LOG_STR("Failed to read ADC value")))
    return NAN;
  return static_cast<float>(value & 0x0FFF);
}

float M5StackIOE1Component::read_adc_ref_() {
  uint16_t value;
  if (!this->status_comm_result(this->read_byte_16_le(REG_REF_VOLT_L, &value),
                                LOG_STR("Failed to read ADC reference voltage")))
    return NAN;
  return static_cast<float>(value);
}

float M5StackIOE1Component::read_temperature_() {
  if (!this->status_comm_result(this->write_byte(REG_TEMP_CTRL, TEMP_START),
                                LOG_STR("Failed to start temperature conversion")))
    return NAN;

  uint8_t status = 0;
  for (uint8_t i = 0; i < CONVERSION_POLL_TRIES; i++) {
    delay(1);
    if (!this->status_comm_result(this->read_byte(REG_TEMP_CTRL, &status),
                                  LOG_STR("Failed to read temperature status")))
      return NAN;
    if ((status & TEMP_BUSY) == 0)
      break;
  }
  if (status & TEMP_BUSY) {
    ESP_LOGW(TAG, "Temperature conversion timed out");
    this->status_set_warning(LOG_STR("Temperature conversion timed out"));
    return NAN;
  }

  uint16_t value;
  if (!this->status_comm_result(this->read_byte_16_le(REG_TEMP_D_L, &value),
                                LOG_STR("Failed to read temperature value")))
    return NAN;
  // The datasheet specifies the 12-bit value in degrees Celsius
  return static_cast<float>(value & 0x0FFF);
}

void M5StackIOE1Component::config_neopixel_(uint8_t num) {
  if (num > MAX_LEDS)
    num = MAX_LEDS;

  // Set the LED count and refresh the RAM data
  if (!this->status_comm_result(this->write_byte(REG_LED_CFG, static_cast<uint8_t>((num & LED_NUM_MASK) | LED_REFRESH)),
                                LOG_STR("Failed to configure NeoPixel LEDs")))
    return;
}

void M5StackIOE1Component::write_led_ram_(const uint8_t *buf, size_t len) {
  for (size_t offset = 0; offset < len; offset += I2C_WRITE_CHUNK_SIZE) {
    const size_t chunk = std::min(I2C_WRITE_CHUNK_SIZE, len - offset);
    if (!this->status_comm_result(
            this->write_bytes(REG_LED_RAM_START + offset, buf + offset, static_cast<uint8_t>(chunk)),
            LOG_STR("Failed to write LED RAM")))
      return;
  }

  uint8_t cfg;
  if (!this->status_comm_result(this->read_byte(REG_LED_CFG, &cfg), LOG_STR("Failed to read LED configuration")))
    return;
  if (!this->status_comm_result(this->write_byte(REG_LED_CFG, cfg | LED_REFRESH), LOG_STR("Failed to refresh LEDs")))
    return;

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char logbuf[256];
  size_t pos = 0;
  for (size_t i = 0; i < len && pos < sizeof(logbuf) - 4; i++) {
    pos += snprintf(logbuf + pos, sizeof(logbuf) - pos, "%02X ", buf[i]);
  }
  ESP_LOGV(TAG, "Wrote LED buffer[%u]: %s", static_cast<unsigned>(len), logbuf);
#endif
}

void M5StackIOE1Component::config_aw8737a_(uint8_t pin, uint8_t pulse_num) {
  // GPIO number in bits 4:0, pulse count in bits 6:5, trigger in bit 7
  const uint8_t data = (pin & 0x1F) | ((pulse_num & AW8737A_PULSE_MASK) << 5);
  if (!this->status_comm_result(this->write_byte(REG_AW8737A_PULSE, data), LOG_STR("Failed to configure AW8737A")))
    return;
}

void M5StackIOE1Component::enable_aw8737a_pulse_(bool enabled) {
  uint8_t data;
  if (!this->status_comm_result(this->read_byte(REG_AW8737A_PULSE, &data), LOG_STR("Failed to read AW8737A control")))
    return;

  assign_bits(data, AW8737A_REFRESH, enabled);
  if (!this->status_comm_result(this->write_byte(REG_AW8737A_PULSE, data), LOG_STR("Failed to write AW8737A control")))
    return;
}

}  // namespace esphome::m5stack_ioe1

ESPHOME_EXPANDER_PIN_SUMMARY(esphome::m5stack_ioe1::M5StackIOE1Component, "%u via M5STACK_IOE1")
