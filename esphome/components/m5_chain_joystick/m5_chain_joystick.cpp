#include "m5_chain_joystick.h"

#include <algorithm>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::m5_chain_joystick {

ESPHOME_LOG_TAG(TAG, "m5_chain_joystick");

static constexpr uint8_t RGB_SET_COMMAND =
    static_cast<uint8_t>(m5_chain_base::ChainCommand::CHAIN_COMMAND_SET_RGB_VALUE);
static constexpr uint8_t RGB_LIGHT_COMMAND =
    static_cast<uint8_t>(m5_chain_base::ChainCommand::CHAIN_COMMAND_SET_RGB_LIGHT);
static constexpr uint8_t BUTTON_REPORT_COMMAND = 0xE0;

void M5ChainJoystick::setup() {
  ESP_LOGCONFIG(TAG, "Setting up M5Stack Chain Joystick (chain_index=%u)...", this->get_chain_index());
  if (this->is_update_interval_enabled()) {
    this->set_interval(POLL_INTERVAL_ID, this->get_update_interval(), [this]() { this->poll_next_(); });
    this->poll_next_();
  }
}

void M5ChainJoystick::dump_config() {
  this->dump_base_config();
  ESP_LOGCONFIG(TAG, "M5Stack Chain Joystick:");
  ESP_LOGCONFIG(TAG, "  Chain index: %u", this->get_chain_index());
}

void M5ChainJoystick::set_joystick_sensor(JoystickSensorType type, uint8_t axis, uint8_t range_index,
                                          sensor::Sensor *sensor) {
  const auto type_index = static_cast<size_t>(type);
  if (type == JoystickSensorType::JOYSTICK_SENSOR_MAPPING_RANGE) {
    if (range_index < this->mapping_range_sensors_.size())
      this->mapping_range_sensors_[range_index] = sensor;
  } else if (type_index < this->sensors_.size() && axis < AXIS_COUNT) {
    this->sensors_[type_index][axis] = sensor;
  }
}

bool M5ChainJoystick::has_sensor_type_(size_t type) const {
  if (type == static_cast<size_t>(JoystickSensorType::JOYSTICK_SENSOR_MAPPING_RANGE))
    return std::any_of(this->mapping_range_sensors_.begin(), this->mapping_range_sensors_.end(),
                       [](const auto *sensor) { return sensor != nullptr; });
  if (type >= this->sensors_.size())
    return false;
  return this->sensors_[type][0] != nullptr || this->sensors_[type][1] != nullptr;
}

void M5ChainJoystick::poll_next_() {
  if (this->request_pending())
    return;
  constexpr size_t poll_slot_count = SENSOR_TYPE_COUNT + 1;
  for (size_t attempt = 0; attempt < poll_slot_count; attempt++) {
    const auto slot = this->poll_slot_++ % poll_slot_count;
    if (slot == SENSOR_TYPE_COUNT) {
      if (this->button_sensor_ == nullptr)
        continue;
      this->pending_type_ = static_cast<uint8_t>(slot);
      const auto status =
          this->request(this->get_chain_index(),
                        static_cast<uint8_t>(JoystickCommand::JOYSTICK_COMMAND_GET_BUTTON_STATUS), RESPONSE_TIMEOUT_MS);
      if (status != m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
        this->status_set_warning();
      return;
    }
    if (!this->has_sensor_type_(slot))
      continue;
    this->pending_type_ = static_cast<uint8_t>(slot);
    uint8_t command = 0;
    switch (slot) {
      case static_cast<size_t>(JoystickSensorType::JOYSTICK_SENSOR_ADC_16):
        command = static_cast<uint8_t>(JoystickCommand::JOYSTICK_COMMAND_GET_ADC_16);
        break;
      case static_cast<size_t>(JoystickSensorType::JOYSTICK_SENSOR_ADC_8):
        command = static_cast<uint8_t>(JoystickCommand::JOYSTICK_COMMAND_GET_ADC_8);
        break;
      case static_cast<size_t>(JoystickSensorType::JOYSTICK_SENSOR_MAPPED_16):
        command = static_cast<uint8_t>(JoystickCommand::JOYSTICK_COMMAND_GET_MAPPED_16);
        break;
      case static_cast<size_t>(JoystickSensorType::JOYSTICK_SENSOR_MAPPED_8):
        command = static_cast<uint8_t>(JoystickCommand::JOYSTICK_COMMAND_GET_MAPPED_8);
        break;
      case static_cast<size_t>(JoystickSensorType::JOYSTICK_SENSOR_MAPPING_RANGE):
        command = static_cast<uint8_t>(JoystickCommand::JOYSTICK_COMMAND_GET_MAPPING_RANGE);
        break;
      default:
        continue;
    }
    const auto status = this->request(this->get_chain_index(), command, RESPONSE_TIMEOUT_MS);
    if (status != m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
      this->status_set_warning();
    return;
  }
}

m5_chain_base::ChainStatus M5ChainJoystick::set_rgb_color(uint8_t red, uint8_t green, uint8_t blue) {
  this->rgb_red_ = red;
  this->rgb_green_ = green;
  this->rgb_blue_ = blue;
  this->rgb_update_pending_ = true;
  return this->send_pending_rgb_();
}

m5_chain_base::ChainStatus M5ChainJoystick::set_rgb_color_component(uint8_t component, uint8_t value) {
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

m5_chain_base::ChainStatus M5ChainJoystick::send_pending_rgb_() {
  if (!this->rgb_update_pending_ || this->request_pending())
    return m5_chain_base::ChainStatus::CHAIN_STATUS_OK;
  const uint8_t data[] = {0, 1, this->rgb_red_, this->rgb_green_, this->rgb_blue_};
  const auto status = this->request(this->get_chain_index(), RGB_SET_COMMAND, data, sizeof(data));
  if (status == m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
    this->rgb_update_pending_ = false;
  return status;
}

m5_chain_base::ChainStatus M5ChainJoystick::set_led_brightness(uint8_t brightness) {
  if (brightness > 100)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {brightness, 0};
  return this->request(this->get_chain_index(), RGB_LIGHT_COMMAND, data, sizeof(data));
}

void M5ChainJoystick::on_response(const m5_chain_base::ChainFrame &frame) {
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
  this->handle_response_(frame, true);
}

void M5ChainJoystick::on_unsolicited(const m5_chain_base::ChainFrame &frame) {
  if (frame.index != this->get_chain_index())
    return;
  if (frame.command == BUTTON_REPORT_COMMAND) {
    ESP_LOGD(TAG, "Received Chain Joystick button event (type=%u)", frame.data_size > 0 ? frame.data[0] : 0);
  } else {
    this->handle_response_(frame, false);
  }
}

void M5ChainJoystick::handle_response_(const m5_chain_base::ChainFrame &frame, bool continue_polling) {
  const auto command = static_cast<JoystickCommand>(frame.command);
  if (command == JoystickCommand::JOYSTICK_COMMAND_GET_BUTTON_STATUS) {
    this->handle_button_status_(frame);
  } else if (command == JoystickCommand::JOYSTICK_COMMAND_GET_ADC_16 ||
             command == JoystickCommand::JOYSTICK_COMMAND_GET_MAPPED_16) {
    if (frame.data_size < 4) {
      this->status_set_warning();
    } else {
      const auto type = command == JoystickCommand::JOYSTICK_COMMAND_GET_ADC_16
                            ? JoystickSensorType::JOYSTICK_SENSOR_ADC_16
                            : JoystickSensorType::JOYSTICK_SENSOR_MAPPED_16;
      const auto type_index = static_cast<size_t>(type);
      const int32_t x = command == JoystickCommand::JOYSTICK_COMMAND_GET_MAPPED_16
                            ? static_cast<int16_t>(encode_uint16(frame.data[1], frame.data[0]))
                            : encode_uint16(frame.data[1], frame.data[0]);
      const int32_t y = command == JoystickCommand::JOYSTICK_COMMAND_GET_MAPPED_16
                            ? static_cast<int16_t>(encode_uint16(frame.data[3], frame.data[2]))
                            : encode_uint16(frame.data[3], frame.data[2]);
      if (this->sensors_[type_index][0] != nullptr)
        this->sensors_[type_index][0]->publish_state(x);
      if (this->sensors_[type_index][1] != nullptr)
        this->sensors_[type_index][1]->publish_state(y);
      this->timeout_warning_active_ = false;
      this->status_clear_warning();
    }
  } else if (command == JoystickCommand::JOYSTICK_COMMAND_GET_ADC_8 ||
             command == JoystickCommand::JOYSTICK_COMMAND_GET_MAPPED_8) {
    if (frame.data_size < 2) {
      this->status_set_warning();
    } else {
      const auto type = command == JoystickCommand::JOYSTICK_COMMAND_GET_ADC_8
                            ? JoystickSensorType::JOYSTICK_SENSOR_ADC_8
                            : JoystickSensorType::JOYSTICK_SENSOR_MAPPED_8;
      const auto type_index = static_cast<size_t>(type);
      const int16_t x = command == JoystickCommand::JOYSTICK_COMMAND_GET_MAPPED_8 ? static_cast<int8_t>(frame.data[0])
                                                                                  : frame.data[0];
      const int16_t y = command == JoystickCommand::JOYSTICK_COMMAND_GET_MAPPED_8 ? static_cast<int8_t>(frame.data[1])
                                                                                  : frame.data[1];
      if (this->sensors_[type_index][0] != nullptr)
        this->sensors_[type_index][0]->publish_state(x);
      if (this->sensors_[type_index][1] != nullptr)
        this->sensors_[type_index][1]->publish_state(y);
      this->timeout_warning_active_ = false;
      this->status_clear_warning();
    }
  } else if (command == JoystickCommand::JOYSTICK_COMMAND_GET_MAPPING_RANGE) {
    if (frame.data_size < 16) {
      this->status_set_warning();
    } else {
      for (size_t index = 0; index < this->mapping_range_sensors_.size(); index++) {
        if (this->mapping_range_sensors_[index] != nullptr)
          this->mapping_range_sensors_[index]->publish_state(
              encode_uint16(frame.data[index * 2 + 1], frame.data[index * 2]));
      }
      this->timeout_warning_active_ = false;
      this->status_clear_warning();
    }
  }
  if (continue_polling) {
    this->send_pending_rgb_();
    this->poll_next_();
  }
}

void M5ChainJoystick::handle_button_status_(const m5_chain_base::ChainFrame &frame) {
  if (frame.data_size < 1) {
    this->status_set_warning();
    return;
  }
  if (this->button_sensor_ != nullptr)
    this->button_sensor_->publish_state(frame.data[0] != 0);
  this->timeout_warning_active_ = false;
  this->status_clear_warning();
}

void M5ChainJoystick::on_request_timeout(uint8_t index, uint8_t command) {
  if (!this->timeout_warning_active_) {
    ESP_LOGW(TAG, "Timeout waiting for Chain Joystick response (index=%u, command=0x%02X)", index, command);
    this->timeout_warning_active_ = true;
  }
  this->status_set_warning();
  this->send_pending_rgb_();
  this->poll_next_();
}

}  // namespace esphome::m5_chain_joystick
