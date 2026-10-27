#include "m5_chain_key.h"

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome::m5_chain_key {

ESPHOME_LOG_TAG(TAG, "m5_chain_key");

static constexpr uint8_t KEY_COMMAND_GET_STATUS = 0xE1;

void M5ChainKey::setup() {
  ESP_LOGCONFIG(TAG, "Setting up M5Stack Chain Key (chain_index=%u)...", this->get_chain_index());
  if (this->is_update_interval_enabled()) {
    this->set_interval(KEY_POLL_INTERVAL_ID, this->get_update_interval(), [this]() { this->poll_key_(); });
    this->poll_key_();
  }
}

void M5ChainKey::dump_config() {
  this->dump_base_config();
  ESP_LOGCONFIG(TAG, "M5Stack Chain Key:");
  ESP_LOGCONFIG(TAG, "  Chain index: %u", this->get_chain_index());
}

void M5ChainKey::poll_key_() {
  if (this->key_sensor_ == nullptr || this->request_pending())
    return;
  if (this->request(this->get_chain_index(), KEY_COMMAND_GET_STATUS, KEY_RESPONSE_TIMEOUT_MS) !=
      m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
    this->status_set_warning();
}

m5_chain_base::ChainStatus M5ChainKey::set_rgb_color(uint8_t red, uint8_t green, uint8_t blue) {
  this->rgb_red_ = red;
  this->rgb_green_ = green;
  this->rgb_blue_ = blue;
  this->rgb_update_pending_ = true;
  return this->send_pending_rgb_();
}

m5_chain_base::ChainStatus M5ChainKey::set_rgb_color_component(uint8_t component, uint8_t value) {
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

m5_chain_base::ChainStatus M5ChainKey::send_pending_rgb_() {
  if (!this->rgb_update_pending_)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_OK;
  if (this->request_pending())
    return m5_chain_base::ChainStatus::CHAIN_STATUS_OK;

  const uint8_t data[] = {0, 1, this->rgb_red_, this->rgb_green_, this->rgb_blue_};
  const auto status = this->request(this->get_chain_index(), m5_chain_base::ChainCommand::CHAIN_COMMAND_SET_RGB_VALUE,
                                    data, sizeof(data));
  if (status == m5_chain_base::ChainStatus::CHAIN_STATUS_OK) {
    this->rgb_update_pending_ = false;
  }
  return status;
}

m5_chain_base::ChainStatus M5ChainKey::set_led_brightness(uint8_t brightness) {
  if (brightness > 100)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {brightness, 0};
  return this->request(this->get_chain_index(), m5_chain_base::ChainCommand::CHAIN_COMMAND_SET_RGB_LIGHT, data,
                       sizeof(data));
}

void M5ChainKey::on_response(const m5_chain_base::ChainFrame &frame) {
  if (frame.command == KEY_COMMAND_GET_STATUS) {
    this->handle_key_status_(frame);
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
  if (frame.command != KEY_COMMAND_GET_STATUS || frame.data_size < 1) {
    this->status_set_warning();
  }
}

void M5ChainKey::on_unsolicited(const m5_chain_base::ChainFrame &frame) {
  // A response can arrive after the request timeout when it traverses several chain nodes.
  // Treat a late key-status response exactly like an on-time response so polling still updates
  // the binary sensor and clears the communication warning.
  if (frame.index == this->get_chain_index() && frame.command == KEY_COMMAND_GET_STATUS) {
    this->handle_key_status_(frame);
  }
}

void M5ChainKey::handle_key_status_(const m5_chain_base::ChainFrame &frame) {
  if (frame.data_size < 1) {
    this->status_set_warning();
    return;
  }
  if (this->key_sensor_ != nullptr)
    this->key_sensor_->publish_state(frame.data[0] != 0);
  this->timeout_warning_active_ = false;
  this->status_clear_warning();
  this->send_pending_rgb_();
}

void M5ChainKey::on_request_timeout(uint8_t index, uint8_t command) {
  if (!this->timeout_warning_active_) {
    ESP_LOGW(TAG, "Timeout waiting for Chain Key response (index=%u, command=0x%02X)", index, command);
    this->timeout_warning_active_ = true;
  }
  this->status_set_warning();
}

}  // namespace esphome::m5_chain_key
