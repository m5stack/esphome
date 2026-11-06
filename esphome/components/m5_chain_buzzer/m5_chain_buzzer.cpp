#include "m5_chain_buzzer.h"

#include "esphome/core/log.h"

namespace esphome::m5_chain_buzzer {

ESPHOME_LOG_TAG(TAG, "m5_chain_buzzer");

static constexpr uint8_t BUZZER_COMMAND_SET_MODE = static_cast<uint8_t>(BuzzerCommand::BUZZER_COMMAND_SET_MODE);
static constexpr uint8_t BUZZER_COMMAND_PLAY_AUTO = static_cast<uint8_t>(BuzzerCommand::BUZZER_COMMAND_PLAY_AUTO);
static constexpr uint8_t BUZZER_COMMAND_SET_FREQUENCY =
    static_cast<uint8_t>(BuzzerCommand::BUZZER_COMMAND_SET_FREQUENCY);
static constexpr uint8_t BUZZER_COMMAND_SET_DUTY = static_cast<uint8_t>(BuzzerCommand::BUZZER_COMMAND_SET_DUTY);
static constexpr uint8_t BUZZER_COMMAND_SET_STATUS = static_cast<uint8_t>(BuzzerCommand::BUZZER_COMMAND_SET_STATUS);
static constexpr uint8_t BUZZER_COMMAND_PLAY_NOTE = static_cast<uint8_t>(BuzzerCommand::BUZZER_COMMAND_PLAY_NOTE);
static constexpr uint8_t RGB_SET_COMMAND =
    static_cast<uint8_t>(m5_chain_base::ChainCommand::CHAIN_COMMAND_SET_RGB_VALUE);
static constexpr uint8_t RGB_LIGHT_COMMAND =
    static_cast<uint8_t>(m5_chain_base::ChainCommand::CHAIN_COMMAND_SET_RGB_LIGHT);

// Held tones use the module's longest auto-play duration (~65 s); the next frame
// replaces the tone long before it expires. A 1 ms duty-0 frame is the silence.
static constexpr uint16_t TONE_HOLD_DURATION_MS = 0xFFFF;
static constexpr uint16_t TONE_SILENT_FREQUENCY = 1000;
static constexpr uint16_t TONE_SILENT_DURATION_MS = 1;

void M5ChainBuzzer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up M5Stack Chain Buzzer (chain_index=%u)...", this->get_chain_index());
}

void M5ChainBuzzer::dump_config() {
  this->dump_base_config();
  ESP_LOGCONFIG(TAG, "M5Stack Chain Buzzer:");
  ESP_LOGCONFIG(TAG, "  Chain index: %u", this->get_chain_index());
}

m5_chain_base::ChainStatus M5ChainBuzzer::set_mode(BuzzerMode mode) {
  const uint8_t data[] = {static_cast<uint8_t>(mode)};
  return this->request(this->get_chain_index(), BUZZER_COMMAND_SET_MODE, data, sizeof(data));
}

m5_chain_base::ChainStatus M5ChainBuzzer::play_auto(uint16_t frequency, uint8_t duty, uint16_t duration_ms) {
  if (frequency < 100 || frequency > 10000 || duty > 100)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {static_cast<uint8_t>(frequency), static_cast<uint8_t>(frequency >> 8), duty,
                          static_cast<uint8_t>(duration_ms), static_cast<uint8_t>(duration_ms >> 8)};
  return this->request(this->get_chain_index(), BUZZER_COMMAND_PLAY_AUTO, data, sizeof(data));
}

m5_chain_base::ChainStatus M5ChainBuzzer::set_frequency(uint16_t frequency) {
  if (frequency < 100 || frequency > 10000)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {static_cast<uint8_t>(frequency), static_cast<uint8_t>(frequency >> 8)};
  return this->request(this->get_chain_index(), BUZZER_COMMAND_SET_FREQUENCY, data, sizeof(data));
}

m5_chain_base::ChainStatus M5ChainBuzzer::set_duty(uint8_t duty) {
  if (duty > 100)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  return this->request(this->get_chain_index(), BUZZER_COMMAND_SET_DUTY, &duty, 1);
}

m5_chain_base::ChainStatus M5ChainBuzzer::set_status(bool enabled) {
  const uint8_t data[] = {static_cast<uint8_t>(enabled)};
  return this->request(this->get_chain_index(), BUZZER_COMMAND_SET_STATUS, data, sizeof(data));
}

m5_chain_base::ChainStatus M5ChainBuzzer::play_note(uint8_t note, uint16_t duration_ms) {
  const uint8_t data[] = {note, static_cast<uint8_t>(duration_ms), static_cast<uint8_t>(duration_ms >> 8)};
  return this->request(this->get_chain_index(), BUZZER_COMMAND_PLAY_NOTE, data, sizeof(data));
}

void M5ChainBuzzer::set_tone(uint16_t frequency, uint8_t duty) {
  if (frequency < 100 || frequency > 10000 || duty > 100)
    return;
  const bool changed = !this->tone_active_ || frequency != this->tone_frequency_ || duty != this->tone_duty_;
  this->tone_active_ = true;
  this->tone_frequency_ = frequency;
  this->tone_duty_ = duty;
  if (changed)
    this->tone_pending_ = true;
  this->service_tone_();
}

void M5ChainBuzzer::set_tone_off() {
  if (!this->tone_active_ && !this->tone_pending_)
    return;
  this->tone_active_ = false;
  this->tone_pending_ = true;
  this->service_tone_();
}

void M5ChainBuzzer::service_tone_() {
  if (!this->tone_pending_ || this->request_pending())
    return;

  // An auto-play frame stops once its duration has elapsed, and SET_STATUS only
  // takes effect in manual mode, so a held tone uses the longest duration and a
  // following note simply replaces it. Duty 0 silences the timer compare, which
  // is the only way to cut an auto-play frame short.
  const auto status = this->tone_active_
                          ? this->play_auto(this->tone_frequency_, this->tone_duty_, TONE_HOLD_DURATION_MS)
                          : this->play_auto(TONE_SILENT_FREQUENCY, 0, TONE_SILENT_DURATION_MS);
  if (status == m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
    this->tone_pending_ = false;
}

void M5ChainBuzzer::poll_next_() { this->service_tone_(); }

m5_chain_base::ChainStatus M5ChainBuzzer::set_rgb_color(uint8_t red, uint8_t green, uint8_t blue) {
  this->rgb_red_ = red;
  this->rgb_green_ = green;
  this->rgb_blue_ = blue;
  this->rgb_update_pending_ = true;
  return this->send_pending_rgb_();
}

m5_chain_base::ChainStatus M5ChainBuzzer::set_rgb_color_component(uint8_t component, uint8_t value) {
  if (component > 2)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  if (component == 0)
    this->rgb_red_ = value;
  else if (component == 1)
    this->rgb_green_ = value;
  else
    this->rgb_blue_ = value;
  this->rgb_update_pending_ = true;
  return this->send_pending_rgb_();
}

m5_chain_base::ChainStatus M5ChainBuzzer::send_pending_rgb_() {
  if (!this->rgb_update_pending_ || this->request_pending())
    return m5_chain_base::ChainStatus::CHAIN_STATUS_OK;
  const uint8_t data[] = {0, 1, this->rgb_red_, this->rgb_green_, this->rgb_blue_};
  const auto status = this->request(this->get_chain_index(), RGB_SET_COMMAND, data, sizeof(data));
  if (status == m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
    this->rgb_update_pending_ = false;
  return status;
}

m5_chain_base::ChainStatus M5ChainBuzzer::set_led_brightness(uint8_t brightness) {
  if (brightness > 100)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {brightness, 0};
  return this->request(this->get_chain_index(), RGB_LIGHT_COMMAND, data, sizeof(data));
}

void M5ChainBuzzer::on_response(const m5_chain_base::ChainFrame &frame) {
  if (frame.command == RGB_SET_COMMAND || frame.command == RGB_LIGHT_COMMAND) {
    if (frame.data_size < 1 || frame.data[0] == 0) {
      this->status_set_warning();
      this->rgb_update_pending_ = false;
    } else {
      this->timeout_warning_active_ = false;
      this->status_clear_warning();
      this->send_pending_rgb_();
    }
    this->service_tone_();
    return;
  }
  if (frame.data_size < 1 || frame.data[0] == 0)
    this->status_set_warning();
  else {
    this->timeout_warning_active_ = false;
    this->status_clear_warning();
  }
  this->send_pending_rgb_();
  this->service_tone_();
}

void M5ChainBuzzer::on_request_timeout(uint8_t index, uint8_t command) {
  if (!this->timeout_warning_active_) {
    ESP_LOGW(TAG, "Timeout waiting for Chain Buzzer response (index=%u, command=0x%02X)", index, command);
    this->timeout_warning_active_ = true;
  }
  this->status_set_warning();
  this->send_pending_rgb_();
  this->service_tone_();
}

}  // namespace esphome::m5_chain_buzzer
