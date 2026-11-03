#include "m5_chain_mic.h"

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::m5_chain_mic {

ESPHOME_LOG_TAG(TAG, "m5_chain_mic");

static constexpr uint8_t MIC_COMMAND_GET_ADC_12 = static_cast<uint8_t>(MicCommand::MIC_COMMAND_GET_ADC_12);
static constexpr uint8_t MIC_COMMAND_GET_ADC_8 = static_cast<uint8_t>(MicCommand::MIC_COMMAND_GET_ADC_8);
static constexpr uint8_t MIC_COMMAND_SET_THRESHOLD = static_cast<uint8_t>(MicCommand::MIC_COMMAND_SET_THRESHOLD);
static constexpr uint8_t MIC_COMMAND_SET_REPORT_MODE = static_cast<uint8_t>(MicCommand::MIC_COMMAND_SET_REPORT_MODE);
static constexpr uint8_t MIC_COMMAND_SET_TRIGGER_INTERVAL =
    static_cast<uint8_t>(MicCommand::MIC_COMMAND_SET_TRIGGER_INTERVAL);
static constexpr uint8_t MIC_COMMAND_TRIGGER_REPORT = static_cast<uint8_t>(MicCommand::MIC_COMMAND_TRIGGER_REPORT);
/// Report type code that marks a Chain MIC threshold report.
static constexpr uint8_t MIC_TRIGGER_REPORT_TYPE = 0x04;

void M5ChainMic::setup() {
  ESP_LOGCONFIG(TAG, "Setting up M5Stack Chain MIC (chain_index=%u)...", this->get_chain_index());
}

void M5ChainMic::dump_config() {
  this->dump_base_config();
  ESP_LOGCONFIG(TAG, "M5Stack Chain MIC:");
  ESP_LOGCONFIG(TAG, "  Chain index: %u", this->get_chain_index());
}

void M5ChainMic::set_adc_sensor(MicSensorType type, sensor::Sensor *sensor) {
  const auto slot = static_cast<size_t>(type);
  if (slot < this->sensors_.size())
    this->sensors_[slot] = sensor;
}

void M5ChainMic::poll_next_() {
  if (this->request_pending())
    return;
  for (size_t attempt = 0; attempt < MIC_SENSOR_SLOT_COUNT; attempt++) {
    const auto slot = this->poll_slot_++ % MIC_SENSOR_SLOT_COUNT;
    if (this->sensors_[slot] == nullptr)
      continue;
    const uint8_t command =
        slot == static_cast<size_t>(MicSensorType::MIC_SENSOR_ADC_8) ? MIC_COMMAND_GET_ADC_8 : MIC_COMMAND_GET_ADC_12;
    if (this->request(this->get_chain_index(), command) != m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
      this->status_set_warning();
    return;
  }
}

m5_chain_base::ChainStatus M5ChainMic::set_threshold(uint16_t threshold, bool save_to_flash) {
  if (threshold > MIC_ADC_12_MAX)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {static_cast<uint8_t>(threshold & 0xFF), static_cast<uint8_t>(threshold >> 8),
                          static_cast<uint8_t>(save_to_flash)};
  return this->request(this->get_chain_index(), MIC_COMMAND_SET_THRESHOLD, data, sizeof(data));
}

m5_chain_base::ChainStatus M5ChainMic::set_report_mode(bool enabled) {
  const uint8_t data[] = {static_cast<uint8_t>(enabled)};
  return this->request(this->get_chain_index(), MIC_COMMAND_SET_REPORT_MODE, data, sizeof(data));
}

m5_chain_base::ChainStatus M5ChainMic::set_trigger_interval(uint16_t interval_ms) {
  if (interval_ms < MIC_TRIGGER_INTERVAL_MIN_MS || interval_ms > MIC_TRIGGER_INTERVAL_MAX_MS)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {static_cast<uint8_t>(interval_ms & 0xFF), static_cast<uint8_t>(interval_ms >> 8)};
  return this->request(this->get_chain_index(), MIC_COMMAND_SET_TRIGGER_INTERVAL, data, sizeof(data));
}

m5_chain_base::ChainStatus M5ChainMic::set_rgb_color(uint8_t red, uint8_t green, uint8_t blue) {
  this->rgb_red_ = red;
  this->rgb_green_ = green;
  this->rgb_blue_ = blue;
  this->rgb_update_pending_ = true;
  return this->send_pending_rgb_();
}

m5_chain_base::ChainStatus M5ChainMic::set_rgb_color_component(uint8_t component, uint8_t value) {
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

m5_chain_base::ChainStatus M5ChainMic::send_pending_rgb_() {
  if (!this->rgb_update_pending_ || this->request_pending())
    return m5_chain_base::ChainStatus::CHAIN_STATUS_OK;

  const uint8_t data[] = {0, 1, this->rgb_red_, this->rgb_green_, this->rgb_blue_};
  const auto status = this->request(this->get_chain_index(), m5_chain_base::ChainCommand::CHAIN_COMMAND_SET_RGB_VALUE,
                                    data, sizeof(data));
  if (status == m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
    this->rgb_update_pending_ = false;
  return status;
}

m5_chain_base::ChainStatus M5ChainMic::set_led_brightness(uint8_t brightness) {
  if (brightness > 100)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {brightness, 0};
  return this->request(this->get_chain_index(), m5_chain_base::ChainCommand::CHAIN_COMMAND_SET_RGB_LIGHT, data,
                       sizeof(data));
}

void M5ChainMic::on_response(const m5_chain_base::ChainFrame &frame) {
  if (frame.command == MIC_COMMAND_GET_ADC_12) {
    this->publish_adc_(frame, MicSensorType::MIC_SENSOR_ADC_12);
    return;
  }
  if (frame.command == MIC_COMMAND_GET_ADC_8) {
    this->publish_adc_(frame, MicSensorType::MIC_SENSOR_ADC_8);
    return;
  }

  if (frame.command == MIC_COMMAND_SET_THRESHOLD || frame.command == MIC_COMMAND_SET_REPORT_MODE ||
      frame.command == MIC_COMMAND_SET_TRIGGER_INTERVAL) {
    if (frame.data_size < 1 || frame.data[0] == 0) {
      this->status_set_warning();
      return;
    }
    this->timeout_warning_active_ = false;
    this->status_clear_warning();
    this->send_pending_rgb_();
    return;
  }

  if (frame.command == static_cast<uint8_t>(m5_chain_base::ChainCommand::CHAIN_COMMAND_SET_RGB_VALUE) ||
      frame.command == static_cast<uint8_t>(m5_chain_base::ChainCommand::CHAIN_COMMAND_SET_RGB_LIGHT)) {
    if (frame.data_size < 1 || frame.data[0] == 0) {
      this->status_set_warning();
      this->rgb_update_pending_ = false;
    } else {
      this->timeout_warning_active_ = false;
      this->status_clear_warning();
      this->send_pending_rgb_();
    }
    return;
  }

  if (frame.data_size < 1 || frame.data[0] == 0)
    this->status_set_warning();
  else {
    this->timeout_warning_active_ = false;
    this->status_clear_warning();
  }
}

void M5ChainMic::on_unsolicited(const m5_chain_base::ChainFrame &frame) {
  if (frame.index != this->get_chain_index())
    return;
  // A response can arrive after the request timeout when it traverses several chain nodes.
  // Treat a late ADC response like an on-time response so polling still updates the sensor.
  if (frame.command == MIC_COMMAND_GET_ADC_12) {
    this->publish_adc_(frame, MicSensorType::MIC_SENSOR_ADC_12);
    return;
  }
  if (frame.command == MIC_COMMAND_GET_ADC_8) {
    this->publish_adc_(frame, MicSensorType::MIC_SENSOR_ADC_8);
    return;
  }
  if (frame.command == MIC_COMMAND_TRIGGER_REPORT) {
    if (frame.data_size < 2 || frame.data[1] != MIC_TRIGGER_REPORT_TYPE) {
      this->status_set_warning();
      return;
    }
    // The device reports 1 when the level rose above the threshold and 0 when it fell back below it.
    const bool loud = frame.data[0] != 0;
    ESP_LOGD(TAG, "Received Chain MIC threshold report (level=%s)",
             loud ? LOG_STR_LITERAL("high") : LOG_STR_LITERAL("low"));
    if (this->trigger_sensor_ != nullptr)
      this->trigger_sensor_->publish_state(loud);
    this->timeout_warning_active_ = false;
    this->status_clear_warning();
  }
}

void M5ChainMic::publish_adc_(const m5_chain_base::ChainFrame &frame, MicSensorType type) {
  const size_t slot = static_cast<size_t>(type);
  const size_t required = type == MicSensorType::MIC_SENSOR_ADC_12 ? 2 : 1;
  if (frame.data_size < required) {
    this->status_set_warning();
    return;
  }
  const float value = type == MicSensorType::MIC_SENSOR_ADC_12 ? encode_uint16(frame.data[1], frame.data[0])
                                                               : static_cast<float>(frame.data[0]);
  if (slot < this->sensors_.size() && this->sensors_[slot] != nullptr)
    this->sensors_[slot]->publish_state(value);
  this->timeout_warning_active_ = false;
  this->status_clear_warning();
  this->send_pending_rgb_();
}

void M5ChainMic::on_request_timeout(uint8_t index, uint8_t command) {
  if (!this->timeout_warning_active_) {
    ESP_LOGW(TAG, "Timeout waiting for Chain MIC response (index=%u, command=0x%02X)", index, command);
    this->timeout_warning_active_ = true;
  }
  this->status_set_warning();
}

}  // namespace esphome::m5_chain_mic
