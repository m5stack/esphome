#include "m5_chain_mono.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome::m5_chain_mono {

ESPHOME_LOG_TAG(TAG, "m5_chain_mono");

static constexpr uint8_t MONO_SET_DISPLAY_BUFFER = static_cast<uint8_t>(MonoCommand::MONO_COMMAND_SET_DISPLAY_BUFFER);
static constexpr uint8_t MONO_SET_BRIGHTNESS = static_cast<uint8_t>(MonoCommand::MONO_COMMAND_SET_BRIGHTNESS);

void M5ChainMono::setup() {
  ESP_LOGCONFIG(TAG, "Setting up M5Stack Chain Mono (chain_index=%u)...", this->get_chain_index());
}

void M5ChainMono::dump_config() {
  this->dump_base_config();
  ESP_LOGCONFIG(TAG, "M5Stack Chain Mono:");
  ESP_LOGCONFIG(TAG, "  Resolution: 8x8");
  ESP_LOGCONFIG(TAG, "  Chain index: %u", this->get_chain_index());
}

m5_chain_base::ChainStatus M5ChainMono::set_display_buffer(const uint8_t *buffer, uint8_t size) {
  if (buffer == nullptr || size != 8)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  std::copy_n(buffer, this->pending_buffer_.size(), this->pending_buffer_.begin());
  this->pending_ = true;
  return this->send_pending_();
}

m5_chain_base::ChainStatus M5ChainMono::set_screen_brightness(uint8_t level) {
  if (level > MAX_SCREEN_BRIGHTNESS_LEVEL)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  if (level == this->brightness_level_)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_OK;
  this->brightness_level_ = level;
  this->pending_brightness_ = level;
  this->brightness_pending_ = true;
  return this->send_pending_();
}

m5_chain_base::ChainStatus M5ChainMono::send_pending_() {
  if (this->request_pending())
    return m5_chain_base::ChainStatus::CHAIN_STATUS_OK;
  if (this->brightness_pending_) {
    // Save_to_flash stays 0: erasing the page closes the UART interrupt for about 20 ms and wears
    // the flash out. The level goes out before a frame so the picture never shows at the old one.
    const uint8_t data[] = {this->pending_brightness_, 0};
    const auto status = this->request(this->get_chain_index(), MONO_SET_BRIGHTNESS, data, sizeof(data));
    if (status == m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
      this->brightness_pending_ = false;
    return status;
  }
  if (!this->pending_)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_OK;
  // A panel frame is idempotent and the next scroll step replaces it, so there is nothing to gain
  // from waiting for the chain node to answer: that wait is what used to stall the whole strip.
  const auto status = this->send_frame(this->get_chain_index(), MONO_SET_DISPLAY_BUFFER, this->pending_buffer_.data(),
                                       this->pending_buffer_.size());
  if (status == m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
    this->pending_ = false;
  return status;
}

void M5ChainMono::on_response(const m5_chain_base::ChainFrame &frame) {
  if ((frame.command != MONO_SET_DISPLAY_BUFFER && frame.command != MONO_SET_BRIGHTNESS) || frame.data_size < 1 ||
      frame.data[0] == 0)
    this->status_set_warning();
  else
    this->status_clear_warning();
  this->send_pending_();
}

void M5ChainMono::on_request_timeout(uint8_t, uint8_t command) {
  this->status_set_warning();
  // The panel is still at the old level, so forget the one that never left: the next write sends
  // it again instead of leaving the picture dimmer than the frontend shows.
  if (command == MONO_SET_BRIGHTNESS)
    this->brightness_level_ = 0xFF;
  this->send_pending_();
}

}  // namespace esphome::m5_chain_mono
