#include "si12t.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#ifdef USE_TEXT_SENSOR
#include "text_sensor/si12t_text_sensor.h"
#endif

namespace esphome::si12t {

ESPHOME_LOG_TAG(TAG, "si12t");

// Register addresses, from the register map in datasheet 12.1
static constexpr uint8_t REG_SEN1 = 0x02;  // SEN1..SEN6 follow it
static constexpr uint8_t REG_SEN_COUNT = 6;
static constexpr uint8_t REG_CFIG = 0x08;
static constexpr uint8_t REG_CTRL = 0x09;
static constexpr uint8_t REG_CH_ON1 = 0x0C;
static constexpr uint8_t REG_OUTPUT1 = 0x10;  // Output1..Output3 follow it
static constexpr uint8_t REG_OUTPUT_COUNT = 3;

// CFIG fields, from datasheet 12.2.2
static constexpr uint8_t CFIG_MS_SHIFT = 7;      // 0: autoselect fast/slow, 1: fast mode
static constexpr uint8_t CFIG_FTC_SHIFT = 5;     // 00: 5 s, 01: 10 s, 10: 15 s, 11: 20 s
static constexpr uint8_t CFIG_ILC_SHIFT = 3;     // interrupt level control
static constexpr uint8_t CFIG_RTC_MASK = 0b111;  // response cycle = RTC + 2
static constexpr uint8_t RESPONSE_CYCLE_OFFSET = 2;
static constexpr uint8_t CFIG_FIELD_MASK = 0b11;

// CTRL, from datasheet 12.2.3: SRST and SLEEP, next to two bits that must stay 1
static constexpr uint8_t CTRL_SRST = 1 << 3;
static constexpr uint8_t CTRL_SLEEP_BIT = 1 << 2;

// Reset value of the six sensitivity registers: ChxHL = 1, ChxM = 011 for both channels of the byte
static constexpr uint8_t SEN_RESET = 0xBB;

// Ch_on1 and Ch_on2 reset values, which hold every channel but channel 1. A zero bit enables a channel,
// and bits 4 and 5 of Ch_on2 are reserved and must stay set (datasheet 12.2.5).
static constexpr uint8_t CH_ON1_RESET = 0xFE;
static constexpr uint8_t CH_ON2_RESET = 0x3F;

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_CONFIG
PROGMEM_STRING_TABLE(ModeStrings, "autoselect", "fast");
PROGMEM_STRING_TABLE(FirstTouchStrings, "5s", "10s", "15s", "20s");
PROGMEM_STRING_TABLE(InterruptLevelStrings, "medium/high", "low/medium/high", "medium/high", "high");
#endif

void IRAM_ATTR SI12TComponent::interrupt_handler(SI12TComponent *arg) {
  arg->pending_ = true;
  arg->enable_loop_soon_any_context();
}

void SI12TComponent::setup() {
  if (!this->setup_check(this->configure_(), LOG_STR("Failed to configure the device")))
    return;

  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    // The interrupt output is open drain and low while a channel is at or above the ILC level, so both
    // edges matter: falling starts a touch, rising ends it. The handler only wakes the loop.
    this->interrupt_pin_->attach_interrupt(&SI12TComponent::interrupt_handler, this, gpio::INTERRUPT_ANY_EDGE);
    this->stop_poller();
  }

  // Report the state the channels are in at boot
  this->update();

  // Polling needs no loop, and neither does an interrupt output that is idle
  if (this->interrupt_pin_ == nullptr || this->interrupt_pin_->digital_read())
    this->disable_loop();
}

bool SI12TComponent::configure_() {
  if (this->software_reset_) {
    // SRST resets the digital part but leaves the I2C module running, so these transfers complete. Release
    // it before configuring anything: a register written while SRST is asserted is reset along with the rest.
    const uint8_t reset_ctrl = this->control_register_ | CTRL_SRST;
    if (this->write_register(REG_CTRL, &reset_ctrl, 1) != i2c::ERROR_OK ||
        this->write_register(REG_CTRL, &this->control_register_, 1) != i2c::ERROR_OK)
      return false;
  } else if (this->write_register(REG_CTRL, &this->control_register_, 1) != i2c::ERROR_OK) {
    return false;
  }

  // SEN1..SEN6 and CFIG in one transfer: each further byte goes to the next register. A SENx register
  // holds two channels, the odd channel number in bits 2:0 and the even one in bits 6:4 (datasheet 12.2.1),
  // and keeps its reset value unless the text sensor of that channel sets the nibble.
  std::array<uint8_t, REG_SEN_COUNT + 1> config;
  config.fill(SEN_RESET);
  uint16_t channel_mask = 0;
#ifdef USE_TEXT_SENSOR
  for (uint8_t channel = 0; channel < CHANNEL_COUNT; channel++) {
    if (this->sensors_[channel] == nullptr)
      continue;
    const uint8_t sensitivity = this->sensors_[channel]->get_sensitivity();
    uint8_t &reg = config[channel / 2];
    if (channel % 2 == 0) {
      reg = (reg & 0xF0) | sensitivity;
    } else {
      reg = (reg & 0x0F) | (sensitivity << 4);
    }
    channel_mask |= 1U << channel;
  }
#endif
  config[REG_CFIG - REG_SEN1] = this->config_register_;
  if (this->write_register(REG_SEN1, config.data(), config.size()) != i2c::ERROR_OK)
    return false;

  // A zero bit enables a channel; the channels without a text sensor keep the reset value, which holds them
  const std::array<uint8_t, 2> ch_on{static_cast<uint8_t>(CH_ON1_RESET & ~(channel_mask & 0xFF)),
                                     static_cast<uint8_t>(CH_ON2_RESET & ~((channel_mask >> 8) & 0x0F))};
  return this->write_register(REG_CH_ON1, ch_on.data(), ch_on.size()) == i2c::ERROR_OK;
}

bool SI12TComponent::read_outputs_() {
  // Output1..Output3 carry all twelve channels at two bits each and are read back to back
  std::array<uint8_t, REG_OUTPUT_COUNT> output{};
  if (this->read_register(REG_OUTPUT1, output.data(), output.size()) != i2c::ERROR_OK)
    return false;
#ifdef USE_TEXT_SENSOR
  // Two bits per channel, four channels per output register (datasheet 12.2.7)
  for (uint8_t channel = 0; channel < CHANNEL_COUNT; channel++) {
    if (this->sensors_[channel] != nullptr)
      this->sensors_[channel]->publish_level((output[channel / 4] >> ((channel % 4) * 2)) & 0b11);
  }
#endif
  return true;
}

void SI12TComponent::update() { this->status_comm_result(this->read_outputs_()); }

void SI12TComponent::loop() {
  // Only an interrupt pin reaches here: setup() stops the poller without one and disables an idle loop
  const uint32_t now = App.get_loop_component_start_time();
  if (!this->pending_ && now - this->last_read_ < this->update_interval_)
    return;
  this->pending_ = false;
  this->last_read_ = now;

  if (!this->status_comm_result(this->read_outputs_()))
    return;

  // The output stays asserted while a channel is at or above the ILC level and there is no state to
  // clear, so keep sampling at the update interval until the touch ends and stop the loop once it is idle.
  if (this->interrupt_pin_->digital_read())
    this->disable_loop();
}

void SI12TComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Si12T:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG,
                "  Mode: %s\n"
                "  First touch: %s\n"
                "  Interrupt level: %s\n"
                "  Response cycles: %u\n"
                "  Sleep: %s\n"
                "  Software reset: %s",
                LOG_STR_ARG(ModeStrings::get_log_str(this->config_register_ >> CFIG_MS_SHIFT, ModeStrings::LAST_INDEX)),
                LOG_STR_ARG(FirstTouchStrings::get_log_str((this->config_register_ >> CFIG_FTC_SHIFT) & CFIG_FIELD_MASK,
                                                           FirstTouchStrings::LAST_INDEX)),
                LOG_STR_ARG(InterruptLevelStrings::get_log_str(
                    (this->config_register_ >> CFIG_ILC_SHIFT) & CFIG_FIELD_MASK, InterruptLevelStrings::LAST_INDEX)),
                (this->config_register_ & CFIG_RTC_MASK) + RESPONSE_CYCLE_OFFSET,
                YESNO(this->control_register_ & CTRL_SLEEP_BIT), YESNO(this->software_reset_));
  if (this->interrupt_pin_ != nullptr)
    LOG_PIN("  Interrupt pin: ", this->interrupt_pin_);
#ifdef USE_TEXT_SENSOR
  for (auto *sensor : this->sensors_) {
    if (sensor != nullptr)
      sensor->dump_channel(TAG);
  }
#endif
}

}  // namespace esphome::si12t
