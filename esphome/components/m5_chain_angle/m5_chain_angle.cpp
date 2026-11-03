#include "m5_chain_angle.h"

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::m5_chain_angle {

ESPHOME_LOG_TAG(TAG, "m5_chain_angle");

static constexpr uint8_t ANGLE_COMMAND_GET_ADC_12 = static_cast<uint8_t>(AngleCommand::ANGLE_COMMAND_GET_ADC_12);
static constexpr uint8_t ANGLE_COMMAND_GET_ADC_8 = static_cast<uint8_t>(AngleCommand::ANGLE_COMMAND_GET_ADC_8);
static constexpr uint8_t RGB_SET_COMMAND =
    static_cast<uint8_t>(m5_chain_base::ChainCommand::CHAIN_COMMAND_SET_RGB_VALUE);
static constexpr uint8_t RGB_LIGHT_COMMAND =
    static_cast<uint8_t>(m5_chain_base::ChainCommand::CHAIN_COMMAND_SET_RGB_LIGHT);

void M5ChainAngle::setup() {
  ESP_LOGCONFIG(TAG, "Setting up M5Stack Chain Angle (chain_index=%u)...", this->get_chain_index());
}

void M5ChainAngle::dump_config() {
  this->dump_base_config();
  ESP_LOGCONFIG(TAG, "M5Stack Chain Angle:");
  ESP_LOGCONFIG(TAG, "  Chain index: %u", this->get_chain_index());
}

void M5ChainAngle::set_angle_sensor(AngleSensorType type, sensor::Sensor *sensor) {
  const auto slot = static_cast<size_t>(type);
  if (slot < this->sensors_.size())
    this->sensors_[slot] = sensor;
}

bool M5ChainAngle::has_sensor_(size_t slot) const {
  return slot < this->sensors_.size() && this->sensors_[slot] != nullptr;
}

void M5ChainAngle::poll_next_() {
  if (this->request_pending())
    return;
  for (size_t attempt = 0; attempt < SENSOR_SLOT_COUNT; attempt++) {
    const auto slot = this->poll_slot_++ % SENSOR_SLOT_COUNT;
    if (!this->has_sensor_(slot))
      continue;
    const uint8_t command = slot == static_cast<size_t>(AngleSensorType::ANGLE_SENSOR_ADC_12) ? ANGLE_COMMAND_GET_ADC_12
                                                                                              : ANGLE_COMMAND_GET_ADC_8;
    this->pending_slot_ = static_cast<uint8_t>(slot);
    if (this->request(this->get_chain_index(), command) != m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
      this->status_set_warning();
    return;
  }
}

m5_chain_base::ChainStatus M5ChainAngle::set_rgb_color(uint8_t red, uint8_t green, uint8_t blue) {
  this->rgb_red_ = red;
  this->rgb_green_ = green;
  this->rgb_blue_ = blue;
  this->rgb_update_pending_ = true;
  return this->send_pending_rgb_();
}

m5_chain_base::ChainStatus M5ChainAngle::set_rgb_color_component(uint8_t component, uint8_t value) {
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

m5_chain_base::ChainStatus M5ChainAngle::send_pending_rgb_() {
  if (!this->rgb_update_pending_ || this->request_pending())
    return m5_chain_base::ChainStatus::CHAIN_STATUS_OK;
  const uint8_t data[] = {0, 1, this->rgb_red_, this->rgb_green_, this->rgb_blue_};
  const auto status = this->request(this->get_chain_index(), RGB_SET_COMMAND, data, sizeof(data));
  if (status == m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
    this->rgb_update_pending_ = false;
  return status;
}

m5_chain_base::ChainStatus M5ChainAngle::set_led_brightness(uint8_t brightness) {
  if (brightness > 100)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {brightness, 0};
  return this->request(this->get_chain_index(), RGB_LIGHT_COMMAND, data, sizeof(data));
}

void M5ChainAngle::on_response(const m5_chain_base::ChainFrame &frame) {
  if (frame.command == RGB_SET_COMMAND || frame.command == RGB_LIGHT_COMMAND) {
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
  this->handle_response_(frame, this->pending_slot_, true);
}

void M5ChainAngle::on_unsolicited(const m5_chain_base::ChainFrame &frame) {
  // A response can arrive after the request timeout when it traverses several chain nodes.
  // Treat a late ADC response like an on-time response so polling still updates the sensor.
  if (frame.index != this->get_chain_index())
    return;
  if (frame.command == ANGLE_COMMAND_GET_ADC_12)
    this->handle_response_(frame, static_cast<uint8_t>(AngleSensorType::ANGLE_SENSOR_ADC_12), false);
  else if (frame.command == ANGLE_COMMAND_GET_ADC_8)
    this->handle_response_(frame, static_cast<uint8_t>(AngleSensorType::ANGLE_SENSOR_ADC_8), false);
}

void M5ChainAngle::handle_response_(const m5_chain_base::ChainFrame &frame, uint8_t slot, bool is_request_response) {
  bool valid = false;
  if (frame.command == ANGLE_COMMAND_GET_ADC_12) {
    if (frame.data_size >= 2) {
      if (this->has_sensor_(slot))
        this->sensors_[slot]->publish_state(encode_uint16(frame.data[1], frame.data[0]));
      valid = true;
    }
  } else if (frame.command == ANGLE_COMMAND_GET_ADC_8) {
    if (frame.data_size >= 1) {
      if (this->has_sensor_(slot))
        this->sensors_[slot]->publish_state(frame.data[0]);
      valid = true;
    }
  }
  if (valid) {
    this->timeout_warning_active_ = false;
    this->status_clear_warning();
  } else {
    this->status_set_warning();
  }
  if (is_request_response)
    this->send_pending_rgb_();
}

void M5ChainAngle::on_request_timeout(uint8_t index, uint8_t command) {
  if (!this->timeout_warning_active_) {
    ESP_LOGW(TAG, "Timeout waiting for Chain Angle response (index=%u, command=0x%02X)", index, command);
    this->timeout_warning_active_ = true;
  }
  this->status_set_warning();
}

}  // namespace esphome::m5_chain_angle
