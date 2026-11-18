#include "aw9523b.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::aw9523b {

static constexpr uint8_t AW9523B_INPUT_P0 = 0x00;     // P0 input status, read only
static constexpr uint8_t AW9523B_INPUT_P1 = 0x01;     // P1 input status, read only
static constexpr uint8_t AW9523B_OUTPUT_P0 = 0x02;    // P0 output status, read/write
static constexpr uint8_t AW9523B_CONFIG_P0 = 0x04;    // P0 input/output configuration, read/write
static constexpr uint8_t AW9523B_INT_P0 = 0x06;       // P0 interrupt enable, read/write
static constexpr uint8_t AW9523B_ID_REG = 0x10;       // chip ID, read only
static constexpr uint8_t AW9523B_CTL = 0x11;          // global control register
static constexpr uint8_t AW9523B_LED_MODE_P0 = 0x12;  // P0 LED mode switch, P1 follows it at 0x13
static constexpr uint8_t AW9523B_DIM_P0 = 0x24;       // P0_n LED current control at 0x24 + n
static constexpr uint8_t AW9523B_DIM_P1_LOW = 0x20;   // P1_n LED current control at 0x20 + n for n < 4
static constexpr uint8_t AW9523B_DIM_P1_HIGH = 0x2C;  // P1_n LED current control at 0x2C + n - 4 for n >= 4
static constexpr uint8_t AW9523B_SW_RST = 0x7F;       // software reset
static constexpr uint8_t AW9523B_CHIP_ID = 0x23;

/// LED current control register of a pin. P1 is not mapped in ascending order: P1_0 to P1_3 sit at
/// 20H to 23H while P1_4 to P1_7 sit at 2CH to 2FH.
static constexpr uint8_t dim_register(uint8_t pin) {
  if (pin < 8)
    return AW9523B_DIM_P0 + pin;
  if (pin < 12)
    return AW9523B_DIM_P1_LOW + (pin - 8);
  return AW9523B_DIM_P1_HIGH + (pin - 12);
}

ESPHOME_LOG_TAG(TAG, "aw9523b");

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
PROGMEM_STRING_TABLE(P0DriveModeStrings, "OPEN_DRAIN", "PUSH_PULL");

static const LogString *p0_drive_mode_to_string(AW9523BP0DriveMode mode) {
  return P0DriveModeStrings::get_log_str(mode, 0);
}

PROGMEM_STRING_TABLE(LEDMaxCurrentStrings, "37mA", "27.75mA", "18.5mA", "9.25mA");

static const LogString *led_max_current_to_string(AW9523BLEDMaxCurrent current) {
  return LEDMaxCurrentStrings::get_log_str(current, 0);
}
#endif

void AW9523BComponent::setup() {
  ESP_LOGD(TAG, "Setting up AW9523B");

  uint8_t chip_id;
  if (!this->read_byte(AW9523B_ID_REG, &chip_id)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  if (chip_id != AW9523B_CHIP_ID) {
    ESP_LOGE(TAG, "Expected chip ID 0x%02X, got 0x%02X", AW9523B_CHIP_ID, chip_id);
    this->mark_failed(LOG_STR("Wrong chip ID"));
    return;
  }

  if (this->reset_) {
    if (!this->write_byte(AW9523B_SW_RST, 0x00)) {
      this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
      return;
    }
    delay(2);  // the software reset takes at least 1 ms
    this->mode_mask_ = 0x0000;
    this->output_mask_ = 0x0000;
    this->interrupt_mask_ = 0xFFFF;
    this->led_mode_mask_ = 0xFFFF;
  } else if (!this->read_gpio_modes_() || !this->read_gpio_outputs_() || !this->read_led_modes_() ||
             !this->read_gpio_interrupts_()) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  // The chip powers up with every interrupt enabled. Disable all of them and let pin_mode() enable
  // the pins that are inputs, so a pin left alone by the configuration cannot latch INTN.
  this->interrupt_mask_ = 0xFFFF;
  if (!this->write_gpio_interrupts_()) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  // GCR: D[1:0] is the global Imax, D4 the P0 push-pull flag. The remaining bits are reserved and
  // have to be written as 0.
  const uint8_t gcr = static_cast<uint8_t>(this->led_max_current_) | (this->p0_drive_mode_ == PUSH_PULL ? (1 << 4) : 0);
  if (!this->write_byte(AW9523B_CTL, gcr)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  // Read both input ports so INTN reports the current pin states from here on.
  if (!this->read_inputs_()) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  this->set_pin_cache_valid_();
  // The cache is refreshed by loop(), which is only enabled while there is something to watch.
  this->set_invalidate_on_read_(false);

  this->disable_loop();
  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->interrupt_pin_->attach_interrupt(&AW9523BComponent::gpio_intr, this, gpio::INTERRUPT_FALLING_EDGE);
    // An edge that happened before the interrupt was attached never fires, so check the line once.
    if (!this->interrupt_pin_->digital_read())
      this->enable_loop();
  }
}

void AW9523BComponent::loop() {
  if (this->interrupt_pin_ != nullptr) {
    // Keep looping on an I2C error so the next pass retries.
    if (!this->read_inputs_())
      return;
    this->set_pin_cache_valid_();
    // Stop only once INTN is released. A transition that straddles the read leaves the line low
    // without another falling edge, so waiting for an edge alone could strand stale states.
    if (this->interrupt_pin_->digital_read())
      this->disable_loop();
    return;
  }

  // Polling mode: one read refreshes both banks, so invalidating once per loop is enough.
  this->reset_pin_cache_();
  if (this->mode_mask_ == 0x0000) {
    this->disable_loop();
  }
}

void AW9523BComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "AW9523B:\n"
                "  P0 drive mode: %s\n"
                "  LED max current: %s",
                LOG_STR_ARG(p0_drive_mode_to_string(this->p0_drive_mode_)),
                LOG_STR_ARG(led_max_current_to_string(this->led_max_current_)));
  LOG_PIN("  Interrupt pin: ", this->interrupt_pin_);
  LOG_I2C_DEVICE(this);
}

void IRAM_ATTR AW9523BComponent::gpio_intr(AW9523BComponent *arg) { arg->enable_loop_soon_any_context(); }

// AW9523B only supports INPUT and OUTPUT pins in GPIO mode
void AW9523BComponent::pin_mode(uint8_t pin, gpio::Flags flags) {
  if (this->is_failed())
    return;

  const uint16_t mask = 1 << pin;
  const bool input = (flags & gpio::FLAG_INPUT) != 0;
  if (((this->mode_mask_ & mask) != 0) == input)
    return;  // already configured this way

  if (input) {
    this->mode_mask_ |= mask;
  } else {
    this->mode_mask_ &= ~mask;
  }
  if (!this->write_gpio_modes_())
    return;

  if (this->interrupt_pin_ != nullptr) {
    this->set_gpio_interrupt_(pin, input);
    if (this->interrupt_mask_ != 0xFFFF) {
      this->enable_loop();
    } else {
      this->disable_loop();
    }
  } else if (input) {
    this->enable_loop();
  } else if (this->mode_mask_ == 0x0000) {
    this->disable_loop();
  }
}

bool AW9523BComponent::read_inputs_() {
  uint8_t p0;
  uint8_t p1;
  // Two separate transactions: the chip only releases INTN once both ports have been read.
  if (!this->read_byte(AW9523B_INPUT_P0, &p0) || !this->read_byte(AW9523B_INPUT_P1, &p1)) {
    this->status_set_warning(LOG_STR("Failed to read input registers"));
    return false;
  }

  this->input_mask_ = p0 | (p1 << 8);
  this->status_clear_warning();
  return true;
}

bool AW9523BComponent::read_gpio_outputs_() {
  if (this->is_failed())
    return false;

  uint8_t data[2];
  if (!this->read_bytes(AW9523B_OUTPUT_P0, data, 2)) {
    this->status_set_warning(LOG_STR("Failed to read output registers"));
    return false;
  }

  this->output_mask_ = data[0] | (data[1] << 8);
  this->status_clear_warning();
  return true;
}

bool AW9523BComponent::read_gpio_modes_() {
  if (this->is_failed())
    return false;

  uint8_t data[2];
  if (!this->read_bytes(AW9523B_CONFIG_P0, data, 2)) {
    this->status_set_warning(LOG_STR("Failed to read GPIO modes"));
    return false;
  }

  this->mode_mask_ = data[0] | (data[1] << 8);
  this->status_clear_warning();
  return true;
}

bool AW9523BComponent::write_gpio_modes_() {
  if (this->is_failed())
    return false;

  const uint8_t data[2] = {static_cast<uint8_t>(this->mode_mask_ & 0xFF), static_cast<uint8_t>(this->mode_mask_ >> 8)};
  if (!this->write_bytes(AW9523B_CONFIG_P0, data, 2)) {
    this->status_set_warning(LOG_STR("Failed to write GPIO modes"));
    return false;
  }

  this->status_clear_warning();
  return true;
}

bool AW9523BComponent::read_gpio_interrupts_() {
  if (this->is_failed())
    return false;

  uint8_t data[2];
  if (!this->read_bytes(AW9523B_INT_P0, data, 2)) {
    this->status_set_warning(LOG_STR("Failed to read interrupt registers"));
    return false;
  }

  this->interrupt_mask_ = data[0] | (data[1] << 8);
  this->status_clear_warning();
  return true;
}

bool AW9523BComponent::write_gpio_interrupts_() {
  if (this->is_failed())
    return false;

  const uint8_t data[2] = {static_cast<uint8_t>(this->interrupt_mask_ & 0xFF),
                           static_cast<uint8_t>(this->interrupt_mask_ >> 8)};
  if (!this->write_bytes(AW9523B_INT_P0, data, 2)) {
    this->status_set_warning(LOG_STR("Failed to write interrupt registers"));
    return false;
  }

  this->status_clear_warning();
  return true;
}

bool AW9523BComponent::digital_read_hw(uint8_t pin) { return this->read_inputs_(); }

bool AW9523BComponent::digital_read_cache(uint8_t pin) { return (this->input_mask_ & (1 << pin)) != 0; }

void AW9523BComponent::digital_write_hw(uint8_t pin, bool value) {
  if (this->is_failed())
    return;

  const uint16_t mask = 1 << pin;
  const uint16_t new_mask = value ? (this->output_mask_ | mask) : (this->output_mask_ & ~mask);
  const uint8_t data[2] = {static_cast<uint8_t>(new_mask & 0xFF), static_cast<uint8_t>(new_mask >> 8)};
  if (!this->write_bytes(AW9523B_OUTPUT_P0, data, 2)) {
    this->status_set_warning(LOG_STR("Failed to write output registers"));
    return;
  }

  this->output_mask_ = new_mask;
  this->status_clear_warning();
}

bool AW9523BComponent::setup_led_mode(uint8_t pin) {
  if (this->is_failed())
    return false;

  const uint16_t mask = 1 << pin;
  if ((this->led_mode_mask_ & mask) == 0)
    return true;  // already in LED mode

  this->led_mode_mask_ &= ~mask;
  if (!this->write_led_modes_())
    return false;

  // A pin in LED mode cannot raise an interrupt, so stop watching it.
  this->set_gpio_interrupt_(pin, false);
  return true;
}

void AW9523BComponent::write_led_current(uint8_t pin, uint8_t current) {
  if (this->is_failed())
    return;

  if (!this->write_byte(dim_register(pin), current)) {
    this->status_set_warning(LOG_STR("Failed to set LED current"));
    return;
  }

  this->status_clear_warning();
}

bool AW9523BComponent::read_led_modes_() {
  if (this->is_failed())
    return false;

  uint8_t data[2];
  if (!this->read_bytes(AW9523B_LED_MODE_P0, data, 2)) {
    this->status_set_warning(LOG_STR("Failed to read LED mode registers"));
    return false;
  }

  this->led_mode_mask_ = data[0] | (data[1] << 8);
  this->status_clear_warning();
  return true;
}

bool AW9523BComponent::write_led_modes_() {
  if (this->is_failed())
    return false;

  const uint8_t data[2] = {static_cast<uint8_t>(this->led_mode_mask_ & 0xFF),
                           static_cast<uint8_t>(this->led_mode_mask_ >> 8)};
  if (!this->write_bytes(AW9523B_LED_MODE_P0, data, 2)) {
    this->status_set_warning(LOG_STR("Failed to write LED mode"));
    return false;
  }

  this->status_clear_warning();
  return true;
}

bool AW9523BComponent::set_gpio_interrupt_(uint8_t pin, bool enabled) {
  const uint16_t mask = 1 << pin;
  if (((this->interrupt_mask_ & mask) == 0) == enabled)
    return true;  // already configured this way

  // INT registers: 0 enables the interrupt of a pin, 1 disables it.
  if (enabled) {
    this->interrupt_mask_ &= ~mask;
  } else {
    this->interrupt_mask_ |= mask;
  }
  return this->write_gpio_interrupts_();
}

}  // namespace esphome::aw9523b
