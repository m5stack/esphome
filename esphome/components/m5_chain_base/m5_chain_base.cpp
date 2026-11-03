#include "m5_chain_base.h"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstring>

#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::m5_chain_base {

ESPHOME_LOG_TAG(TAG, "m5_chain_base");

static constexpr size_t MAX_CHAIN_INSTANCES = 16;
static std::array<M5ChainBase *, MAX_CHAIN_INSTANCES> chain_instances{};

struct ChainBusState {
  uart::UARTComponent *parent{nullptr};
  M5ChainBase *active_request{nullptr};
  size_t round_robin_cursor{0};
  /// Earliest millis() at which the next frame may start. Nothing may be written before it.
  uint32_t next_allowed_at{0};
};

static std::array<ChainBusState, MAX_CHAIN_INSTANCES> chain_buses{};

static ChainBusState *get_bus_state(uart::UARTComponent *parent, bool create) {
  if (parent == nullptr)
    return nullptr;
  for (auto &bus : chain_buses) {
    if (bus.parent == parent)
      return &bus;
  }
  if (create) {
    for (auto &bus : chain_buses) {
      if (bus.parent == nullptr) {
        bus.parent = parent;
        return &bus;
      }
    }
  }
  return nullptr;
}

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
      if (get_bus_state(this->parent_, true) == nullptr) {
        ESP_LOGW(TAG, "Too many M5Stack Chain UART buses; requests may not be scheduled");
      }
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
  auto *bus = get_bus_state(this->parent_, false);
  if (bus == nullptr)
    return;
  if (bus->active_request == this)
    bus->active_request = nullptr;
  for (const auto *instance : chain_instances) {
    if (instance != nullptr && instance->parent_ == this->parent_)
      return;
  }
  *bus = {};
}

void M5ChainBase::setup() { this->register_instance_(); }

void M5ChainBase::loop() {
  if (this->parent_ == nullptr)
    return;

  // One instance drains and schedules each shared UART bus.
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
      if (this->receive_buffer_size_ == this->receive_buffer_.size()) {
        this->parse_buffer_();
        if (this->receive_buffer_size_ == this->receive_buffer_.size()) {
          ESP_LOGW(TAG, "Chain RX buffer full; discarding incomplete frame");
          this->receive_buffer_size_ = 0;
        }
      }
      this->receive_buffer_[this->receive_buffer_size_++] = byte;
      if (this->receive_buffer_size_ >= MAX_PACKET_SIZE)
        this->parse_buffer_();
    }
    this->parse_buffer_();
    this->service_bus_(millis());
  }
}

void M5ChainBase::dump_config() { this->dump_base_config(); }

void M5ChainBase::dump_base_config() {
  ESP_LOGCONFIG(TAG, "M5Stack Chain UART base");
  ESP_LOGCONFIG(TAG, "  Bus arbitration: round-robin");
  ESP_LOGCONFIG(TAG, "  Inter-request delay: %" PRIu32 " ms", INTER_REQUEST_DELAY_MS);
  ESP_LOGCONFIG(TAG, "  Pipelined frame pacing: %" PRIu32 " ms after the frame leaves the wire", PIPELINE_PACING_MS);
  ESP_LOGCONFIG(TAG, "  Response timeout: %" PRIu32 " ms + %" PRIu32 " ms per chain hop", BASE_RESPONSE_TIMEOUT_MS,
                PER_CHAIN_HOP_TIMEOUT_MS);
  ESP_LOGCONFIG(TAG, "  Request retries: %u", MAX_REQUEST_RETRIES);
  if (this->update_interval_ms_ == SCHEDULER_DONT_RUN) {
    ESP_LOGCONFIG(TAG, "  Update interval: never");
  } else {
    ESP_LOGCONFIG(TAG, "  Update interval: %" PRIu32 " ms", this->update_interval_ms_);
  }
}

bool M5ChainBase::send_packet(uint8_t index, uint8_t command, const uint8_t *data, uint16_t data_size) {
  auto *bus = get_bus_state(this->parent_, false);
  const auto now = millis();
  if (this->parent_ == nullptr || bus == nullptr || bus->active_request != nullptr ||
      static_cast<int32_t>(now - bus->next_allowed_at) < 0)
    return false;
  for (const auto *instance : chain_instances) {
    if (instance != nullptr && instance->parent_ == this->parent_ && instance->request_queued_)
      return false;
  }
  uint16_t packet_size = 0;
  if (!build_packet(index, command, data, data_size, this->send_buffer_.data(), this->send_buffer_.size(),
                    &packet_size)) {
    return false;
  }
  this->write_array(this->send_buffer_.data(), packet_size);
  bus->next_allowed_at = now + wire_time_ms_(packet_size) + INTER_REQUEST_DELAY_MS;
  return true;
}

ChainStatus M5ChainBase::request(uint8_t index, uint8_t command, const uint8_t *data, uint16_t data_size,
                                 uint32_t timeout_ms) {
  if (this->request_pending_)
    return ChainStatus::CHAIN_STATUS_BUSY;
  if (timeout_ms == 0)
    return ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  if (this->parent_ == nullptr)
    return ChainStatus::CHAIN_STATUS_NOT_INITIALIZED;
  auto *bus = get_bus_state(this->parent_, false);
  if (bus == nullptr)
    return ChainStatus::CHAIN_STATUS_NOT_INITIALIZED;
  if (timeout_ms == DEFAULT_REQUEST_TIMEOUT_MS)
    timeout_ms = response_timeout_for_index_(index);
  if (!build_packet(index, command, data, data_size, this->send_buffer_.data(), this->send_buffer_.size(),
                    &this->send_buffer_size_))
    return ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;

  this->pending_index_ = index;
  this->pending_command_ = command;
  this->request_timeout_ms_ = timeout_ms;
  this->expect_response_ = true;
  this->request_pending_ = true;
  this->request_queued_ = true;
  this->transaction_active_ = false;
  this->request_started_at_ = 0;
  this->retry_at_ = millis();
  this->retry_count_ = 0;
  this->start_queued_request_(this->retry_at_);
  return ChainStatus::CHAIN_STATUS_OK;
}

uint32_t M5ChainBase::response_timeout_for_index_(uint8_t index) {
  const uint32_t hop_count = index > 0 ? index - 1 : 0;
  return BASE_RESPONSE_TIMEOUT_MS + hop_count * PER_CHAIN_HOP_TIMEOUT_MS;
}

uint32_t M5ChainBase::wire_time_ms_(uint16_t packet_size) {
  return (static_cast<uint32_t>(packet_size) * BITS_PER_BYTE * 1000 + CHAIN_BAUD_RATE - 1) / CHAIN_BAUD_RATE;
}

ChainStatus M5ChainBase::send_frame(uint8_t index, uint8_t command, const uint8_t *data, uint16_t data_size) {
  if (this->request_pending_ || this->request_queued_)
    return ChainStatus::CHAIN_STATUS_BUSY;
  if (this->parent_ == nullptr)
    return ChainStatus::CHAIN_STATUS_NOT_INITIALIZED;
  auto *bus = get_bus_state(this->parent_, false);
  if (bus == nullptr)
    return ChainStatus::CHAIN_STATUS_NOT_INITIALIZED;
  if (!build_packet(index, command, data, data_size, this->send_buffer_.data(), this->send_buffer_.size(),
                    &this->send_buffer_size_))
    return ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;

  this->pending_index_ = index;
  this->pending_command_ = command;
  this->expect_response_ = false;
  this->request_pending_ = true;
  this->request_queued_ = true;
  this->transaction_active_ = false;
  this->request_started_at_ = 0;
  this->retry_at_ = millis();
  this->retry_count_ = 0;
  this->start_queued_request_(this->retry_at_);
  return ChainStatus::CHAIN_STATUS_OK;
}

bool M5ChainBase::start_queued_request_(uint32_t now) {
  if (!this->request_pending_ || !this->request_queued_)
    return false;
  auto *bus = get_bus_state(this->parent_, false);
  if (bus == nullptr || bus->active_request != nullptr || static_cast<int32_t>(now - this->retry_at_) < 0 ||
      static_cast<int32_t>(now - bus->next_allowed_at) < 0)
    return false;

  this->write_array(this->send_buffer_.data(), this->send_buffer_size_);
  this->request_queued_ = false;

  if (!this->expect_response_) {
    // Nothing waits on the acknowledgement, so hand the bus straight back: the next panel only has
    // to wait for this frame to clear the wire plus one hop of forwarding time.
    this->request_pending_ = false;
    this->transaction_active_ = false;
    bus->next_allowed_at = now + wire_time_ms_(this->send_buffer_size_) + PIPELINE_PACING_MS;
  } else {
    this->transaction_active_ = true;
    this->request_started_at_ = now;
    bus->active_request = this;
    bus->next_allowed_at = now + wire_time_ms_(this->send_buffer_size_);
  }

  for (size_t i = 0; i < chain_instances.size(); i++) {
    if (chain_instances[i] == this) {
      bus->round_robin_cursor = (i + 1) % chain_instances.size();
      break;
    }
  }
  return true;
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
  // Only a frame that answers one of our requests throttles the bus. Unmatched traffic, such as the
  // acknowledgement of a send_frame() call, must not delay the next panel by another inter-request
  // gap, otherwise pipelining would gain nothing.
  bool matched = false;
  for (auto *instance : chain_instances) {
    if (instance != nullptr && instance->parent_ == this->parent_)
      matched |= instance->dispatch_frame_to_instance_(packet, packet_size);
  }
  if (!matched)
    return;
  if (auto *bus = get_bus_state(this->parent_, false); bus != nullptr) {
    const uint32_t allowed = millis() + INTER_REQUEST_DELAY_MS;
    if (static_cast<int32_t>(allowed - bus->next_allowed_at) > 0)
      bus->next_allowed_at = allowed;
  }
}

bool M5ChainBase::dispatch_frame_to_instance_(const uint8_t *packet, uint16_t packet_size) {
  const uint16_t length = encode_uint16(packet[3], packet[2]);
  const ChainFrame frame{packet[4], packet[5], packet + 6, static_cast<uint16_t>(length - MIN_PACKET_LENGTH)};
  const bool matches_request = this->request_pending_ && this->expect_response_ &&
                               (this->transaction_active_ || this->retry_count_ > 0) &&
                               frame.index == this->pending_index_ && frame.command == this->pending_command_;
  if (matches_request) {
    // A slow answer still works, but it steals time from the next frame on a shared bus.
    const uint32_t round_trip = millis() - this->request_started_at_;
    if (round_trip >= SLOW_RESPONSE_LOG_MS) {
      ESP_LOGD(TAG, "Slow response from index %u for command 0x%02X: %" PRIu32 " ms", frame.index, frame.command,
               round_trip);
    }
    this->request_pending_ = false;
    this->request_queued_ = false;
    this->transaction_active_ = false;
    this->retry_count_ = 0;
    if (auto *bus = get_bus_state(this->parent_, false); bus != nullptr && bus->active_request == this)
      bus->active_request = nullptr;
    this->on_response(frame);
  } else {
    this->on_unsolicited(frame);
  }
  (void) packet_size;
  return matches_request;
}

void M5ChainBase::discard_prefix_(uint16_t count) {
  count = std::min(count, this->receive_buffer_size_);
  const uint16_t remaining = this->receive_buffer_size_ - count;
  if (remaining > 0)
    std::memmove(this->receive_buffer_.data(), this->receive_buffer_.data() + count, remaining);
  this->receive_buffer_size_ = remaining;
}

void M5ChainBase::handle_timeout_(uint32_t now) {
  if (!this->transaction_active_ || now - this->request_started_at_ < this->request_timeout_ms_)
    return;
  auto *bus = get_bus_state(this->parent_, false);
  if (bus != nullptr && bus->active_request == this)
    bus->active_request = nullptr;
  this->transaction_active_ = false;
  if (bus != nullptr) {
    const uint32_t allowed = now + INTER_REQUEST_DELAY_MS;
    if (static_cast<int32_t>(allowed - bus->next_allowed_at) > 0)
      bus->next_allowed_at = allowed;
  }
  if (this->retry_count_ < MAX_REQUEST_RETRIES) {
    const uint32_t backoff = RETRY_BACKOFF_MS << this->retry_count_;
    this->retry_count_++;
    this->retry_at_ = now + backoff;
    this->request_queued_ = true;
    ESP_LOGW(TAG, "No response from index %u for command 0x%02X after %" PRIu32 " ms, retry %u/%u",
             this->pending_index_, this->pending_command_, this->request_timeout_ms_, this->retry_count_,
             MAX_REQUEST_RETRIES);
    return;
  }

  const uint8_t index = this->pending_index_;
  const uint8_t command = this->pending_command_;
  this->request_pending_ = false;
  this->request_queued_ = false;
  this->retry_count_ = 0;
  ESP_LOGW(TAG, "No response from index %u for command 0x%02X, giving up", index, command);
  this->on_request_timeout(index, command);
}

void M5ChainBase::service_bus_(uint32_t now) {
  auto *bus = get_bus_state(this->parent_, false);
  if (bus == nullptr)
    return;

  if (bus->active_request != nullptr) {
    bus->active_request->handle_timeout_(now);
    if (bus->active_request != nullptr)
      return;
  }
  if (static_cast<int32_t>(now - bus->next_allowed_at) < 0)
    return;

  // Polls go before queued work: a device that re-queues as soon as its response arrives (the
  // display and light frames of Chain Mono/RGB do) would otherwise keep the bus busy forever and
  // the instances that only poll every update interval would never get a turn.
  for (size_t offset = 0; offset < chain_instances.size(); offset++) {
    const size_t index = (bus->round_robin_cursor + offset) % chain_instances.size();
    auto *instance = chain_instances[index];
    if (instance == nullptr || instance->parent_ != this->parent_ || instance->request_pending_ ||
        !instance->is_update_interval_enabled() || static_cast<int32_t>(now - instance->next_poll_at_) < 0)
      continue;
    instance->next_poll_at_ = now + instance->update_interval_ms_;
    instance->poll_next_();
    if (instance->request_pending_) {
      if (instance->request_queued_)
        instance->start_queued_request_(now);
      return;
    }
  }

  for (size_t offset = 0; offset < chain_instances.size(); offset++) {
    const size_t index = (bus->round_robin_cursor + offset) % chain_instances.size();
    auto *instance = chain_instances[index];
    if (instance == nullptr || instance->parent_ != this->parent_ || !instance->request_pending_ ||
        !instance->request_queued_ || static_cast<int32_t>(now - instance->retry_at_) < 0)
      continue;
    if (instance->start_queued_request_(now))
      return;
  }
}

}  // namespace esphome::m5_chain_base
