#include "m5_chain_tof.h"

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::m5_chain_tof {

ESPHOME_LOG_TAG(TAG, "m5_chain_tof");

static constexpr uint8_t TOF_COMMAND_GET_DISTANCE = static_cast<uint8_t>(TofCommand::TOF_COMMAND_GET_DISTANCE);

void M5ChainToF::setup() {
  ESP_LOGCONFIG(TAG, "Setting up M5Stack Chain ToF (chain_index=%u)...", this->get_chain_index());
}

void M5ChainToF::dump_config() {
  this->dump_base_config();
  ESP_LOGCONFIG(TAG, "M5Stack Chain ToF:");
  ESP_LOGCONFIG(TAG, "  Chain index: %u", this->get_chain_index());
}

void M5ChainToF::poll_next_() {
  if (this->distance_sensor_ == nullptr || this->request_pending())
    return;
  if (this->request(this->get_chain_index(), static_cast<uint8_t>(TofCommand::TOF_COMMAND_GET_DISTANCE)) !=
      m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
    this->status_set_warning();
}

m5_chain_base::ChainStatus M5ChainToF::set_rgb_color(uint8_t red, uint8_t green, uint8_t blue) {
  this->rgb_red_ = red;
  this->rgb_green_ = green;
  this->rgb_blue_ = blue;
  this->rgb_update_pending_ = true;
  return this->send_pending_rgb_();
}

m5_chain_base::ChainStatus M5ChainToF::set_rgb_color_component(uint8_t component, uint8_t value) {
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

m5_chain_base::ChainStatus M5ChainToF::send_pending_rgb_() {
  if (!this->rgb_update_pending_)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_OK;
  if (this->request_pending())
    return m5_chain_base::ChainStatus::CHAIN_STATUS_OK;

  const uint8_t data[] = {0, 1, this->rgb_red_, this->rgb_green_, this->rgb_blue_};
  const auto status = this->request(this->get_chain_index(), m5_chain_base::ChainCommand::CHAIN_COMMAND_SET_RGB_VALUE,
                                    data, sizeof(data));
  if (status == m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
    this->rgb_update_pending_ = false;
  return status;
}

m5_chain_base::ChainStatus M5ChainToF::set_led_brightness(uint8_t brightness) {
  if (brightness > 100)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {brightness, 0};
  return this->request(this->get_chain_index(), m5_chain_base::ChainCommand::CHAIN_COMMAND_SET_RGB_LIGHT, data,
                       sizeof(data));
}

void M5ChainToF::on_response(const m5_chain_base::ChainFrame &frame) {
  if (frame.command == TOF_COMMAND_GET_DISTANCE) {
    this->handle_distance_(frame);
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

void M5ChainToF::on_unsolicited(const m5_chain_base::ChainFrame &frame) {
  // A response can arrive after the request timeout when it traverses several chain nodes.
  // Treat a late distance response like an on-time response so polling still updates the sensor.
  if (frame.index == this->get_chain_index() && frame.command == TOF_COMMAND_GET_DISTANCE)
    this->handle_distance_(frame);
}

void M5ChainToF::handle_distance_(const m5_chain_base::ChainFrame &frame) {
  if (frame.data_size < 2) {
    this->status_set_warning();
    return;
  }
  if (this->distance_sensor_ != nullptr)
    this->distance_sensor_->publish_state(encode_uint16(frame.data[1], frame.data[0]));
  this->timeout_warning_active_ = false;
  this->status_clear_warning();
  this->send_pending_rgb_();
}

void M5ChainToF::on_request_timeout(uint8_t index, uint8_t command) {
  if (!this->timeout_warning_active_) {
    ESP_LOGW(TAG, "Timeout waiting for Chain ToF response (index=%u, command=0x%02X)", index, command);
    this->timeout_warning_active_ = true;
  }
  this->status_set_warning();
}

}  // namespace esphome::m5_chain_tof
