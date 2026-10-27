#include "m5_chain_switch.h"

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::m5_chain_switch {

ESPHOME_LOG_TAG(TAG, "m5_chain_switch");

static constexpr uint8_t SWITCH_COMMAND_GET_STATUS = static_cast<uint8_t>(SwitchCommand::SWITCH_COMMAND_GET_STATUS);
static constexpr uint8_t SWITCH_COMMAND_GET_ADC_12 = static_cast<uint8_t>(SwitchCommand::SWITCH_COMMAND_GET_ADC_12);
static constexpr uint8_t SWITCH_COMMAND_GET_ADC_8 = static_cast<uint8_t>(SwitchCommand::SWITCH_COMMAND_GET_ADC_8);
static constexpr uint8_t SWITCH_COMMAND_GET_CHANGE_MODE =
    static_cast<uint8_t>(SwitchCommand::SWITCH_COMMAND_GET_CHANGE_MODE);
static constexpr uint8_t SWITCH_COMMAND_GET_THRESHOLD =
    static_cast<uint8_t>(SwitchCommand::SWITCH_COMMAND_GET_THRESHOLD);
static constexpr uint8_t SWITCH_COMMAND_SET_EVENT_TRIGGER =
    static_cast<uint8_t>(SwitchCommand::SWITCH_COMMAND_SET_EVENT_TRIGGER);
static constexpr uint8_t SWITCH_COMMAND_GET_EVENT_TRIGGER =
    static_cast<uint8_t>(SwitchCommand::SWITCH_COMMAND_GET_EVENT_TRIGGER);
static constexpr uint8_t SWITCH_COMMAND_EVENT_TRIGGER_REPORT = 0xE0;
static constexpr uint8_t SWITCH_EVENT_REPORT_TYPE = 0x04;

void M5ChainSwitch::setup() {
  ESP_LOGCONFIG(TAG, "Setting up M5Stack Chain Switch (chain_index=%u)...", this->get_chain_index());
  if (this->is_update_interval_enabled()) {
    this->set_interval(EVENT_TRIGGER_POLL_INTERVAL_ID, this->get_update_interval(), [this]() { this->poll_next_(); });
    this->poll_next_();
  }
}

void M5ChainSwitch::dump_config() {
  this->dump_base_config();
  ESP_LOGCONFIG(TAG, "M5Stack Chain Switch:");
  ESP_LOGCONFIG(TAG, "  Chain index: %u", this->get_chain_index());
}

void M5ChainSwitch::set_switch_sensor(SwitchSensorType type, sensor::Sensor *sensor) {
  const auto slot = static_cast<size_t>(type);
  if (slot < this->sensors_.size())
    this->sensors_[slot] = sensor;
}

bool M5ChainSwitch::has_sensor_(size_t slot) const {
  return slot < this->sensors_.size() && this->sensors_[slot] != nullptr;
}

void M5ChainSwitch::poll_next_() {
  if (this->request_pending())
    return;

  constexpr size_t status_slot = SENSOR_SLOT_COUNT;
  constexpr size_t event_trigger_slot = SENSOR_SLOT_COUNT + 1;
  constexpr size_t poll_slot_count = SENSOR_SLOT_COUNT + 2;
  for (size_t attempt = 0; attempt < poll_slot_count; attempt++) {
    const auto slot = this->poll_slot_++ % poll_slot_count;
    uint8_t command = 0;
    if (slot < SENSOR_SLOT_COUNT) {
      if (!this->has_sensor_(slot))
        continue;
      switch (slot) {
        case static_cast<size_t>(SwitchSensorType::SWITCH_SENSOR_ADC_12):
          command = SWITCH_COMMAND_GET_ADC_12;
          break;
        case static_cast<size_t>(SwitchSensorType::SWITCH_SENSOR_ADC_8):
          command = SWITCH_COMMAND_GET_ADC_8;
          break;
        case static_cast<size_t>(SwitchSensorType::SWITCH_SENSOR_CHANGE_MODE):
          command = SWITCH_COMMAND_GET_CHANGE_MODE;
          break;
        case static_cast<size_t>(SwitchSensorType::SWITCH_SENSOR_OPEN_THRESHOLD):
          command = SWITCH_COMMAND_GET_THRESHOLD;
          break;
        case static_cast<size_t>(SwitchSensorType::SWITCH_SENSOR_CLOSE_THRESHOLD):
          if (this->has_sensor_(static_cast<size_t>(SwitchSensorType::SWITCH_SENSOR_OPEN_THRESHOLD)))
            continue;
          command = SWITCH_COMMAND_GET_THRESHOLD;
          break;
        default:
          continue;
      }
    } else if (slot == status_slot) {
      if (this->status_sensor_ == nullptr)
        continue;
      command = SWITCH_COMMAND_GET_STATUS;
    } else if (slot == event_trigger_slot) {
      if (this->event_trigger_switch_ == nullptr)
        continue;
      command = SWITCH_COMMAND_GET_EVENT_TRIGGER;
    }
    this->pending_slot_ = static_cast<uint8_t>(slot);
    const auto status = this->request(this->get_chain_index(), command);
    if (status != m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
      this->status_set_warning();
    return;
  }
}

m5_chain_base::ChainStatus M5ChainSwitch::set_event_trigger(bool enabled) {
  const uint8_t data[] = {static_cast<uint8_t>(enabled)};
  const auto status = this->request(this->get_chain_index(), SWITCH_COMMAND_SET_EVENT_TRIGGER, data, sizeof(data));
  if (status == m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
    this->event_trigger_state_pending_ = enabled;
  return status;
}

m5_chain_base::ChainStatus M5ChainSwitch::set_rgb_color(uint8_t red, uint8_t green, uint8_t blue) {
  this->rgb_red_ = red;
  this->rgb_green_ = green;
  this->rgb_blue_ = blue;
  this->rgb_update_pending_ = true;
  return this->send_pending_rgb_();
}

m5_chain_base::ChainStatus M5ChainSwitch::set_rgb_color_component(uint8_t component, uint8_t value) {
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

m5_chain_base::ChainStatus M5ChainSwitch::send_pending_rgb_() {
  if (!this->rgb_update_pending_ || this->request_pending())
    return m5_chain_base::ChainStatus::CHAIN_STATUS_OK;

  const uint8_t data[] = {0, 1, this->rgb_red_, this->rgb_green_, this->rgb_blue_};
  const auto status = this->request(this->get_chain_index(), m5_chain_base::ChainCommand::CHAIN_COMMAND_SET_RGB_VALUE,
                                    data, sizeof(data));
  if (status == m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
    this->rgb_update_pending_ = false;
  return status;
}

m5_chain_base::ChainStatus M5ChainSwitch::set_led_brightness(uint8_t brightness) {
  if (brightness > 100)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {brightness, 0};
  return this->request(this->get_chain_index(), m5_chain_base::ChainCommand::CHAIN_COMMAND_SET_RGB_LIGHT, data,
                       sizeof(data));
}

void M5ChainSwitch::on_response(const m5_chain_base::ChainFrame &frame) {
  if (frame.command == SWITCH_COMMAND_GET_EVENT_TRIGGER) {
    if (frame.data_size >= 1 && this->event_trigger_switch_ != nullptr)
      this->event_trigger_switch_->publish_state(frame.data[0] != 0);
    else if (frame.data_size < 1)
      this->status_set_warning();
    if (frame.data_size >= 1)
      this->status_clear_warning();
    return;
  }

  if (frame.command == SWITCH_COMMAND_SET_EVENT_TRIGGER) {
    if (frame.data_size < 1 || frame.data[0] == 0) {
      this->status_set_warning();
      return;
    }
    if (this->event_trigger_switch_ != nullptr)
      this->event_trigger_switch_->publish_state(this->event_trigger_state_pending_);
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
      this->status_clear_warning();
      this->send_pending_rgb_();
    }
    return;
  }

  this->handle_response_(frame, this->pending_slot_, true);
}

void M5ChainSwitch::on_unsolicited(const m5_chain_base::ChainFrame &frame) {
  if (frame.index != this->get_chain_index())
    return;
  if (frame.command == SWITCH_COMMAND_EVENT_TRIGGER_REPORT) {
    this->handle_status_report_(frame);
    return;
  }
  if (frame.command == SWITCH_COMMAND_GET_ADC_12) {
    this->handle_response_(frame, static_cast<uint8_t>(SwitchSensorType::SWITCH_SENSOR_ADC_12), false);
    return;
  }
  if (frame.command == SWITCH_COMMAND_GET_ADC_8) {
    this->handle_response_(frame, static_cast<uint8_t>(SwitchSensorType::SWITCH_SENSOR_ADC_8), false);
    return;
  }
  if (frame.command == SWITCH_COMMAND_GET_CHANGE_MODE) {
    this->handle_response_(frame, static_cast<uint8_t>(SwitchSensorType::SWITCH_SENSOR_CHANGE_MODE), false);
    return;
  }
  if (frame.command == SWITCH_COMMAND_GET_STATUS) {
    this->handle_response_(frame, static_cast<uint8_t>(SENSOR_SLOT_COUNT), false);
    return;
  }
  if (frame.command == SWITCH_COMMAND_GET_THRESHOLD)
    this->handle_response_(frame, static_cast<uint8_t>(SwitchSensorType::SWITCH_SENSOR_OPEN_THRESHOLD), false);
}

void M5ChainSwitch::on_request_timeout(uint8_t index, uint8_t command) {
  if (!this->timeout_warning_active_) {
    ESP_LOGW(TAG, "Timeout waiting for Chain Switch response (index=%u, command=0x%02X)", index, command);
    this->timeout_warning_active_ = true;
  }
  this->status_set_warning();
  this->poll_next_();
}

void M5ChainSwitch::handle_status_report_(const m5_chain_base::ChainFrame &frame) {
  if (frame.data_size < 2 || frame.data[1] != SWITCH_EVENT_REPORT_TYPE) {
    this->status_set_warning();
    return;
  }
  if (this->status_sensor_ != nullptr)
    this->status_sensor_->publish_state(frame.data[0] != 0);
  ESP_LOGD(TAG, "Received Chain Switch state event (index=%u, type=%u)", frame.index, frame.data[0]);
  this->timeout_warning_active_ = false;
  this->status_clear_warning();
}

void M5ChainSwitch::handle_response_(const m5_chain_base::ChainFrame &frame, uint8_t slot, bool continue_polling) {
  bool valid = false;
  if (frame.command == SWITCH_COMMAND_GET_STATUS) {
    if (frame.data_size >= 1) {
      if (this->status_sensor_ != nullptr)
        this->status_sensor_->publish_state(frame.data[0] != 0);
      valid = true;
    }
  } else if (frame.command == SWITCH_COMMAND_GET_ADC_12 || frame.command == SWITCH_COMMAND_GET_ADC_8 ||
             frame.command == SWITCH_COMMAND_GET_CHANGE_MODE) {
    if (frame.data_size >= (frame.command == SWITCH_COMMAND_GET_ADC_12 ? 2 : 1)) {
      const auto value =
          frame.command == SWITCH_COMMAND_GET_ADC_12 ? encode_uint16(frame.data[1], frame.data[0]) : frame.data[0];
      if (slot < this->sensors_.size() && this->sensors_[slot] != nullptr)
        this->sensors_[slot]->publish_state(value);
      valid = true;
    }
  } else if (frame.command == SWITCH_COMMAND_GET_THRESHOLD) {
    if (frame.data_size >= 4) {
      if (this->sensors_[static_cast<size_t>(SwitchSensorType::SWITCH_SENSOR_OPEN_THRESHOLD)] != nullptr)
        this->sensors_[static_cast<size_t>(SwitchSensorType::SWITCH_SENSOR_OPEN_THRESHOLD)]->publish_state(
            encode_uint16(frame.data[1], frame.data[0]));
      if (this->sensors_[static_cast<size_t>(SwitchSensorType::SWITCH_SENSOR_CLOSE_THRESHOLD)] != nullptr)
        this->sensors_[static_cast<size_t>(SwitchSensorType::SWITCH_SENSOR_CLOSE_THRESHOLD)]->publish_state(
            encode_uint16(frame.data[3], frame.data[2]));
      valid = true;
    }
  }
  if (valid) {
    this->timeout_warning_active_ = false;
    this->status_clear_warning();
  } else {
    this->status_set_warning();
  }
  if (continue_polling) {
    this->send_pending_rgb_();
    this->poll_next_();
  }
}

}  // namespace esphome::m5_chain_switch
