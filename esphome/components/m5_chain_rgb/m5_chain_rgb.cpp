#include "m5_chain_rgb.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome::m5_chain_rgb {

ESPHOME_LOG_TAG(TAG, "m5_chain_rgb");

static constexpr uint8_t RGB_SET_DISPLAY_BUFFER = static_cast<uint8_t>(RgbCommand::RGB_COMMAND_SET_DISPLAY_BUFFER);
static constexpr uint8_t RGB_SET_BRIGHTNESS = static_cast<uint8_t>(RgbCommand::RGB_COMMAND_SET_BRIGHTNESS);

void M5ChainRGB::setup() {
  ESP_LOGCONFIG(TAG, "Setting up M5Stack Chain RGB (chain_index=%u)...", this->get_chain_index());
}

void M5ChainRGB::dump_config() {
  this->dump_base_config();
  ESP_LOGCONFIG(TAG, "M5Stack Chain RGB:");
  ESP_LOGCONFIG(TAG, "  Resolution: 8x8 RGB565");
  ESP_LOGCONFIG(TAG, "  Chain index: %u", this->get_chain_index());
}

m5_chain_base::ChainStatus M5ChainRGB::set_display_buffer(const uint8_t *buffer, uint16_t size) {
  if (buffer == nullptr || size != 128)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  std::copy_n(buffer, this->pending_buffer_.size(), this->pending_buffer_.begin());
  this->pending_ = true;
  return this->send_pending_();
}

m5_chain_base::ChainStatus M5ChainRGB::set_screen_brightness(uint8_t percent) {
  if (percent > MAX_SCREEN_BRIGHTNESS)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  if (percent == this->brightness_percent_)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_OK;
  this->brightness_percent_ = percent;
  this->pending_brightness_ = percent;
  this->brightness_pending_ = true;
  return this->send_pending_();
}

m5_chain_base::ChainStatus M5ChainRGB::send_pending_() {
  if (this->request_pending())
    return m5_chain_base::ChainStatus::CHAIN_STATUS_OK;
  if (this->brightness_pending_) {
    // Save_to_flash stays 0: erasing the page closes the UART interrupt for about 20 ms and wears
    // the flash out. The module redraws its current frame at the new brightness, so the level goes
    // out before the frame and nothing is ever shown at the previous one.
    const uint8_t data[] = {this->pending_brightness_, 0};
    const auto status = this->request(this->get_chain_index(), RGB_SET_BRIGHTNESS, data, sizeof(data));
    if (status == m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
      this->brightness_pending_ = false;
    return status;
  }
  if (!this->pending_)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_OK;
  // See M5ChainMono::send_pending_(): the panel frame is replaced by the next one, so waiting for
  // the chain node's acknowledgement would only add its forwarding latency to every frame.
  const auto status = this->send_frame(this->get_chain_index(), RGB_SET_DISPLAY_BUFFER, this->pending_buffer_.data(),
                                       this->pending_buffer_.size());
  if (status == m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
    this->pending_ = false;
  return status;
}

void M5ChainRGB::on_response(const m5_chain_base::ChainFrame &frame) {
  if ((frame.command != RGB_SET_DISPLAY_BUFFER && frame.command != RGB_SET_BRIGHTNESS) || frame.data_size < 1 ||
      frame.data[0] == 0)
    this->status_set_warning();
  else
    this->status_clear_warning();
  this->send_pending_();
}

void M5ChainRGB::on_request_timeout(uint8_t, uint8_t command) {
  this->status_set_warning();
  // The module is still at the old brightness, so forget the one that never left: the next write
  // sends it again instead of leaving the panel dimmer than the frontend shows.
  if (command == RGB_SET_BRIGHTNESS)
    this->brightness_percent_ = 0xFF;
  this->send_pending_();
}

}  // namespace esphome::m5_chain_rgb
