#include "m5_chain_base.h"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstring>

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::m5_chain_base {

ESPHOME_LOG_TAG(TAG, "m5_chain_base");

static constexpr size_t MAX_CHAIN_INSTANCES = 16;
static std::array<M5ChainBase *, MAX_CHAIN_INSTANCES> chain_instances{};

M5ChainBase::~M5ChainBase() { this->unregister_instance_(); }

void M5ChainBase::set_uart_parent(uart::UARTComponent *parent) {
  this->unregister_instance_();
  this->UARTDevice::set_uart_parent(parent);
  this->register_instance_();
}

void M5ChainBase::register_instance_() {
  if (this->parent_ == nullptr)
    return;
  for (auto *instance : chain_instances) {
    if (instance == this)
      return;
  }
  for (auto &instance : chain_instances) {
    if (instance == nullptr) {
      instance = this;
      return;
    }
  }
  ESP_LOGW(TAG, "Too many M5Stack Chain instances; UART frames may not be dispatched");
}

void M5ChainBase::unregister_instance_() {
  for (auto &instance : chain_instances) {
    if (instance == this)
      instance = nullptr;
  }
}

void M5ChainBase::setup() { this->register_instance_(); }

void M5ChainBase::loop() {
  // One UART has one receive queue. The first registered Chain instance owns
  // draining it, then dispatches each complete frame to every instance on the
  // same UART so a response for a later node is not consumed by an earlier one.
  bool is_reader = false;
  for (auto *instance : chain_instances) {
    if (instance != nullptr && instance->parent_ == this->parent_) {
      is_reader = instance == this;
      break;
    }
  }
  if (is_reader) {
    while (this->available() > 0) {
      uint8_t byte = 0;
      if (!this->read_byte(&byte))
        break;
      if (this->receive_buffer_size_ < this->receive_buffer_.size()) {
        this->receive_buffer_[this->receive_buffer_size_++] = byte;
      } else {
        // Drop the oldest byte so a malformed stream cannot permanently stall parsing.
        std::memmove(this->receive_buffer_.data(), this->receive_buffer_.data() + 1, this->receive_buffer_.size() - 1);
        this->receive_buffer_.back() = byte;
      }
    }
    this->parse_buffer_();
  }
  this->handle_timeout_(App.get_loop_component_start_time());
}

void M5ChainBase::dump_config() { this->dump_base_config(); }

void M5ChainBase::dump_base_config() {
  ESP_LOGCONFIG(TAG, "M5Stack Chain UART base");
  if (this->update_interval_ms_ == SCHEDULER_DONT_RUN) {
    ESP_LOGCONFIG(TAG, "  Update interval: never");
  } else {
    ESP_LOGCONFIG(TAG, "  Update interval: %" PRIu32 " ms", this->update_interval_ms_);
  }
}

bool M5ChainBase::send_packet(uint8_t index, uint8_t command, const uint8_t *data, uint16_t data_size) {
  uint16_t packet_size = 0;
  if (this->parent_ == nullptr || !build_packet(index, command, data, data_size, this->send_buffer_.data(),
                                                this->send_buffer_.size(), &packet_size)) {
    return false;
  }
  this->write_array(this->send_buffer_.data(), packet_size);
  return true;
}

ChainStatus M5ChainBase::request(uint8_t index, uint8_t command, const uint8_t *data, uint16_t data_size,
                                 uint32_t timeout_ms) {
  if (this->request_pending_)
    return ChainStatus::CHAIN_STATUS_BUSY;
  uint16_t packet_size = 0;
  if (timeout_ms == 0 || this->parent_ == nullptr ||
      !build_packet(index, command, data, data_size, this->send_buffer_.data(), this->send_buffer_.size(),
                    &packet_size))
    return timeout_ms == 0 ? ChainStatus::CHAIN_STATUS_PARAMETER_ERROR : ChainStatus::CHAIN_STATUS_NOT_INITIALIZED;

  this->pending_index_ = index;
  this->pending_command_ = command;
  this->request_timeout_ms_ = timeout_ms;
  this->write_array(this->send_buffer_.data(), packet_size);
  this->request_pending_ = true;
  this->request_started_at_ = App.get_loop_component_start_time();
  return ChainStatus::CHAIN_STATUS_OK;
}

uint8_t M5ChainBase::calculate_crc(uint8_t index, uint8_t command, const uint8_t *data, uint16_t data_size) {
  if (data_size > 0 && data == nullptr)
    return 0;
  uint16_t checksum = static_cast<uint16_t>(index) + command;
  for (uint16_t i = 0; i < data_size; i++)
    checksum += data[i];
  return static_cast<uint8_t>(checksum);
}

uint8_t M5ChainBase::calculate_crc(const uint8_t *packet, uint16_t packet_size) {
  if (packet == nullptr || packet_size < MIN_PACKET_SIZE)
    return 0;
  return M5ChainBase::calculate_crc(packet[4], packet[5], packet + 6, packet_size - 9);
}

bool M5ChainBase::build_packet(uint8_t index, uint8_t command, const uint8_t *data, uint16_t data_size, uint8_t *packet,
                               uint16_t packet_capacity, uint16_t *packet_size) {
  if (packet == nullptr || packet_size == nullptr || data_size > MAX_DATA_SIZE || (data_size > 0 && data == nullptr))
    return false;

  const uint16_t length = static_cast<uint16_t>(data_size + MIN_PACKET_LENGTH);
  const uint16_t total_size = static_cast<uint16_t>(length + 6);
  if (packet_capacity < total_size)
    return false;

  packet[0] = PACKET_HEADER_HIGH;
  packet[1] = PACKET_HEADER_LOW;
  packet[2] = static_cast<uint8_t>(length);
  packet[3] = static_cast<uint8_t>(length >> 8);
  packet[4] = index;
  packet[5] = command;
  if (data_size > 0)
    std::memcpy(packet + 6, data, data_size);
  packet[total_size - 3] = calculate_crc(index, command, data, data_size);
  packet[total_size - 2] = PACKET_TAIL_HIGH;
  packet[total_size - 1] = PACKET_TAIL_LOW;
  *packet_size = total_size;
  return true;
}

bool M5ChainBase::validate_packet(const uint8_t *packet, uint16_t packet_size) {
  if (packet == nullptr || packet_size < MIN_PACKET_SIZE || packet_size > MAX_PACKET_SIZE ||
      packet[0] != PACKET_HEADER_HIGH || packet[1] != PACKET_HEADER_LOW ||
      packet[packet_size - 2] != PACKET_TAIL_HIGH || packet[packet_size - 1] != PACKET_TAIL_LOW)
    return false;

  const uint16_t length = encode_uint16(packet[3], packet[2]);
  if (length < MIN_PACKET_LENGTH || length > MAX_PACKET_LENGTH || static_cast<uint32_t>(length) + 6 != packet_size)
    return false;
  return calculate_crc(packet, packet_size) == packet[packet_size - 3];
}

void M5ChainBase::parse_buffer_() {
  while (this->receive_buffer_size_ >= MIN_PACKET_SIZE) {
    uint16_t header_index = 0;
    while (header_index + 1 < this->receive_buffer_size_ &&
           (this->receive_buffer_[header_index] != PACKET_HEADER_HIGH ||
            this->receive_buffer_[header_index + 1] != PACKET_HEADER_LOW)) {
      header_index++;
    }
    if (header_index > 0) {
      this->discard_prefix_(header_index);
      continue;
    }
    if (this->receive_buffer_size_ < 4)
      return;

    const uint16_t length = encode_uint16(this->receive_buffer_[3], this->receive_buffer_[2]);
    const uint32_t packet_size = static_cast<uint32_t>(length) + 6;
    if (length < MIN_PACKET_LENGTH || length > MAX_PACKET_LENGTH || packet_size > this->receive_buffer_.size()) {
      this->discard_prefix_(1);
      continue;
    }
    if (this->receive_buffer_size_ < packet_size)
      return;
    if (!validate_packet(this->receive_buffer_.data(), static_cast<uint16_t>(packet_size))) {
      this->discard_prefix_(1);
      continue;
    }

    this->dispatch_frame_(this->receive_buffer_.data(), static_cast<uint16_t>(packet_size));
    this->discard_prefix_(static_cast<uint16_t>(packet_size));
  }
}

void M5ChainBase::dispatch_frame_(const uint8_t *packet, uint16_t packet_size) {
  for (auto *instance : chain_instances) {
    if (instance != nullptr && instance->parent_ == this->parent_)
      instance->dispatch_frame_to_instance_(packet, packet_size);
  }
}

void M5ChainBase::dispatch_frame_to_instance_(const uint8_t *packet, uint16_t packet_size) {
  const uint16_t length = encode_uint16(packet[3], packet[2]);
  const ChainFrame frame{packet[4], packet[5], packet + 6, static_cast<uint16_t>(length - MIN_PACKET_LENGTH)};
  const bool matches_request =
      this->request_pending_ && frame.index == this->pending_index_ && frame.command == this->pending_command_;
  if (matches_request) {
    this->request_pending_ = false;
    this->on_response(frame);
  } else {
    this->on_unsolicited(frame);
  }
  (void) packet_size;
}

void M5ChainBase::discard_prefix_(uint16_t count) {
  count = std::min(count, this->receive_buffer_size_);
  const uint16_t remaining = this->receive_buffer_size_ - count;
  if (remaining > 0)
    std::memmove(this->receive_buffer_.data(), this->receive_buffer_.data() + count, remaining);
  this->receive_buffer_size_ = remaining;
}

void M5ChainBase::handle_timeout_(uint32_t now) {
  if (!this->request_pending_ || now - this->request_started_at_ < this->request_timeout_ms_)
    return;
  const uint8_t index = this->pending_index_;
  const uint8_t command = this->pending_command_;
  this->request_pending_ = false;
  this->on_request_timeout(index, command);
}

}  // namespace esphome::m5_chain_base
