#include "m5_chain_pir.h"

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome::m5_chain_pir {

ESPHOME_LOG_TAG(TAG, "m5_chain_pir");

static constexpr uint8_t PIR_COMMAND_GET_STATUS = static_cast<uint8_t>(PirCommand::PIR_COMMAND_GET_STATUS);
static constexpr uint8_t PIR_COMMAND_EVENT_REPORT = static_cast<uint8_t>(PirCommand::PIR_COMMAND_EVENT_REPORT);
static constexpr uint8_t PIR_COMMAND_SET_EVENT_REPORT = static_cast<uint8_t>(PirCommand::PIR_COMMAND_SET_EVENT_REPORT);
static constexpr uint8_t PIR_COMMAND_SET_KEEP_SECONDS = static_cast<uint8_t>(PirCommand::PIR_COMMAND_SET_KEEP_SECONDS);
/// Report type code that marks a Chain PIR detection event report.
static constexpr uint8_t PIR_EVENT_REPORT_TYPE = 0x05;

void M5ChainPir::setup() {
  ESP_LOGCONFIG(TAG, "Setting up M5Stack Chain PIR (chain_index=%u)...", this->get_chain_index());
}

void M5ChainPir::dump_config() {
  this->dump_base_config();
  ESP_LOGCONFIG(TAG, "M5Stack Chain PIR:");
  ESP_LOGCONFIG(TAG, "  Chain index: %u", this->get_chain_index());
}

void M5ChainPir::poll_next_() {
  if (this->presence_sensor_ == nullptr || this->request_pending())
    return;
  if (this->request(this->get_chain_index(), PIR_COMMAND_GET_STATUS) != m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
    this->status_set_warning();
}

m5_chain_base::ChainStatus M5ChainPir::set_event_report(bool enabled) {
  const uint8_t data[] = {static_cast<uint8_t>(enabled)};
  return this->request(this->get_chain_index(), PIR_COMMAND_SET_EVENT_REPORT, data, sizeof(data));
}

m5_chain_base::ChainStatus M5ChainPir::set_keep_seconds(uint8_t seconds, bool save_to_flash) {
  const uint8_t data[] = {seconds, static_cast<uint8_t>(save_to_flash)};
  return this->request(this->get_chain_index(), PIR_COMMAND_SET_KEEP_SECONDS, data, sizeof(data));
}

m5_chain_base::ChainStatus M5ChainPir::set_rgb_color(uint8_t red, uint8_t green, uint8_t blue) {
  this->rgb_red_ = red;
  this->rgb_green_ = green;
  this->rgb_blue_ = blue;
  this->rgb_update_pending_ = true;
  return this->send_pending_rgb_();
}

m5_chain_base::ChainStatus M5ChainPir::set_rgb_color_component(uint8_t component, uint8_t value) {
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

m5_chain_base::ChainStatus M5ChainPir::send_pending_rgb_() {
  if (!this->rgb_update_pending_ || this->request_pending())
    return m5_chain_base::ChainStatus::CHAIN_STATUS_OK;

  const uint8_t data[] = {0, 1, this->rgb_red_, this->rgb_green_, this->rgb_blue_};
  const auto status = this->request(this->get_chain_index(), m5_chain_base::ChainCommand::CHAIN_COMMAND_SET_RGB_VALUE,
                                    data, sizeof(data));
  if (status == m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
    this->rgb_update_pending_ = false;
  return status;
}

m5_chain_base::ChainStatus M5ChainPir::set_led_brightness(uint8_t brightness) {
  if (brightness > 100)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {brightness, 0};
  return this->request(this->get_chain_index(), m5_chain_base::ChainCommand::CHAIN_COMMAND_SET_RGB_LIGHT, data,
                       sizeof(data));
}

void M5ChainPir::on_response(const m5_chain_base::ChainFrame &frame) {
  if (frame.command == PIR_COMMAND_GET_STATUS) {
    if (frame.data_size < 1) {
      this->status_set_warning();
      return;
    }
    this->publish_presence_(frame.data[0] != 0);
    this->send_pending_rgb_();
    return;
  }

  if (frame.command == PIR_COMMAND_SET_EVENT_REPORT || frame.command == PIR_COMMAND_SET_KEEP_SECONDS) {
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

void M5ChainPir::on_unsolicited(const m5_chain_base::ChainFrame &frame) {
  // A response can arrive after the request timeout when it traverses several chain nodes.
  // Treat a late status response like an on-time response so polling still updates the sensor.
  if (frame.index == this->get_chain_index()) {
    if (frame.command == PIR_COMMAND_GET_STATUS) {
      if (frame.data_size >= 1)
        this->publish_presence_(frame.data[0] != 0);
      return;
    }
    if (frame.command == PIR_COMMAND_EVENT_REPORT) {
      if (frame.data_size < 2 || frame.data[1] != PIR_EVENT_REPORT_TYPE) {
        this->status_set_warning();
        return;
      }
      ESP_LOGD(TAG, "Received Chain PIR detection event (detected=%s)",
               frame.data[0] != 0 ? LOG_STR_LITERAL("yes") : LOG_STR_LITERAL("no"));
      this->publish_presence_(frame.data[0] != 0);
    }
  }
}

void M5ChainPir::publish_presence_(bool detected) {
  if (this->presence_sensor_ != nullptr)
    this->presence_sensor_->publish_state(detected);
  this->timeout_warning_active_ = false;
  this->status_clear_warning();
}

void M5ChainPir::on_request_timeout(uint8_t index, uint8_t command) {
  if (!this->timeout_warning_active_) {
    ESP_LOGW(TAG, "Timeout waiting for Chain PIR response (index=%u, command=0x%02X)", index, command);
    this->timeout_warning_active_ = true;
  }
  this->status_set_warning();
}

}  // namespace esphome::m5_chain_pir
