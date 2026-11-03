#include "m5_chain_encoder.h"

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::m5_chain_encoder {

ESPHOME_LOG_TAG(TAG, "m5_chain_encoder");

static constexpr uint8_t RGB_SET_COMMAND =
    static_cast<uint8_t>(m5_chain_base::ChainCommand::CHAIN_COMMAND_SET_RGB_VALUE);
static constexpr uint8_t RGB_LIGHT_COMMAND =
    static_cast<uint8_t>(m5_chain_base::ChainCommand::CHAIN_COMMAND_SET_RGB_LIGHT);
static constexpr uint8_t BUTTON_REPORT_COMMAND = 0xE0;

void M5ChainEncoder::setup() {
  ESP_LOGCONFIG(TAG, "Setting up M5Stack Chain Encoder (chain_index=%u)...", this->get_chain_index());
}

void M5ChainEncoder::dump_config() {
  this->dump_base_config();
  ESP_LOGCONFIG(TAG, "M5Stack Chain Encoder:");
  ESP_LOGCONFIG(TAG, "  Chain index: %u", this->get_chain_index());
}

void M5ChainEncoder::set_encoder_sensor(EncoderSensorType type, sensor::Sensor *sensor) {
  const auto slot = static_cast<size_t>(type);
  if (slot < this->sensors_.size())
    this->sensors_[slot] = sensor;
}

void M5ChainEncoder::poll_next_() {
  if (this->request_pending())
    return;
  for (size_t attempt = 0; attempt < this->sensors_.size() + 1; attempt++) {
    const uint8_t slot = this->poll_slot_++ % static_cast<uint8_t>(this->sensors_.size() + 1);
    uint8_t command = 0;
    if (slot < this->sensors_.size()) {
      if (this->sensors_[slot] == nullptr)
        continue;
      command = static_cast<uint8_t>(EncoderCommand::ENCODER_COMMAND_GET_VALUE) + (slot == 0 ? 0 : slot == 1 ? 1 : 6);
    } else {
      if (this->button_sensor_ == nullptr)
        continue;
      command = static_cast<uint8_t>(EncoderCommand::ENCODER_COMMAND_BUTTON_GET_STATUS);
    }
    this->pending_slot_ = slot;
    const auto status = this->request(this->get_chain_index(), command);
    if (status != m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
      this->status_set_warning();
    return;
  }
}

m5_chain_base::ChainStatus M5ChainEncoder::set_rgb_color(uint8_t red, uint8_t green, uint8_t blue) {
  this->rgb_red_ = red;
  this->rgb_green_ = green;
  this->rgb_blue_ = blue;
  this->rgb_update_pending_ = true;
  return this->send_pending_rgb_();
}

m5_chain_base::ChainStatus M5ChainEncoder::set_rgb_color_component(uint8_t component, uint8_t value) {
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

m5_chain_base::ChainStatus M5ChainEncoder::send_pending_rgb_() {
  if (!this->rgb_update_pending_ || this->request_pending())
    return m5_chain_base::ChainStatus::CHAIN_STATUS_OK;
  const uint8_t data[] = {0, 1, this->rgb_red_, this->rgb_green_, this->rgb_blue_};
  const auto status = this->request(this->get_chain_index(), RGB_SET_COMMAND, data, sizeof(data));
  if (status == m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
    this->rgb_update_pending_ = false;
  return status;
}

m5_chain_base::ChainStatus M5ChainEncoder::set_led_brightness(uint8_t brightness) {
  if (brightness > 100)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {brightness, 0};
  return this->request(this->get_chain_index(), RGB_LIGHT_COMMAND, data, sizeof(data));
}

void M5ChainEncoder::on_response(const m5_chain_base::ChainFrame &frame) {
  if (frame.command == RGB_SET_COMMAND || frame.command == RGB_LIGHT_COMMAND) {
    if (frame.data_size < 1 || frame.data[0] == 0) {
      this->status_set_warning();
      this->rgb_update_pending_ = false;
    } else {
      this->status_clear_warning();
      this->send_pending_rgb_();
    }
    return;
  }
  if (this->pending_slot_ == 3) {
    if (frame.data_size < 1) {
      this->status_set_warning();
    } else if (this->button_sensor_ != nullptr) {
      this->button_sensor_->publish_state(frame.data[0] != 0);
      this->status_clear_warning();
    }
    this->send_pending_rgb_();
    return;
  }
  const size_t slot = this->pending_slot_;
  if (slot >= this->sensors_.size() || this->sensors_[slot] == nullptr || frame.data_size < 1) {
    this->status_set_warning();
    this->send_pending_rgb_();
    return;
  }
  if (slot < 2) {
    if (frame.data_size < 2) {
      this->status_set_warning();
    } else {
      this->sensors_[slot]->publish_state(static_cast<int16_t>(encode_uint16(frame.data[1], frame.data[0])));
      this->status_clear_warning();
    }
  } else {
    this->sensors_[slot]->publish_state(frame.data[0] != 0);
    this->status_clear_warning();
  }
  this->send_pending_rgb_();
}

void M5ChainEncoder::on_unsolicited(const m5_chain_base::ChainFrame &frame) {
  if (frame.command == BUTTON_REPORT_COMMAND && frame.data_size >= 1 && frame.index == this->get_chain_index()) {
    ESP_LOGD(TAG, "Received Chain Encoder button event (type=%u)", frame.data[0]);
  }
}

void M5ChainEncoder::on_request_timeout(uint8_t index, uint8_t command) {
  ESP_LOGW(TAG, "Timeout waiting for Chain Encoder response (index=%u, command=0x%02X)", index, command);
  this->status_set_warning();
  this->send_pending_rgb_();
}

}  // namespace esphome::m5_chain_encoder
