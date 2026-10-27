#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome::m5_chain_base {

static constexpr uint8_t PACKET_HEADER_HIGH = 0xAA;
static constexpr uint8_t PACKET_HEADER_LOW = 0x55;
static constexpr uint8_t PACKET_TAIL_HIGH = 0x55;
static constexpr uint8_t PACKET_TAIL_LOW = 0xAA;
static constexpr uint16_t MAX_PACKET_LENGTH = 0x100;
static constexpr uint16_t MAX_PACKET_SIZE = MAX_PACKET_LENGTH + 6;
static constexpr uint16_t MIN_PACKET_LENGTH = 3;
static constexpr uint16_t MIN_PACKET_SIZE = MIN_PACKET_LENGTH + 6;
static constexpr uint16_t MAX_DATA_SIZE = MAX_PACKET_LENGTH - MIN_PACKET_LENGTH;

enum class ChainStatus : uint8_t {
  CHAIN_STATUS_OK = 0x00,
  CHAIN_STATUS_PARAMETER_ERROR = 0x01,
  CHAIN_STATUS_RETURN_PACKET_ERROR = 0x02,
  CHAIN_STATUS_NOT_INITIALIZED = 0x03,
  CHAIN_STATUS_BUSY = 0x04,
  CHAIN_STATUS_TIMEOUT = 0x05,
};

enum class ChainCommand : uint8_t {
  CHAIN_COMMAND_SET_RGB_VALUE = 0x20,
  CHAIN_COMMAND_GET_RGB_VALUE = 0x21,
  CHAIN_COMMAND_SET_RGB_LIGHT = 0x22,
  CHAIN_COMMAND_GET_RGB_LIGHT = 0x23,
  CHAIN_COMMAND_GET_UID = 0xF8,
  CHAIN_COMMAND_GET_BOOTLOADER_VERSION = 0xF9,
  CHAIN_COMMAND_GET_FIRMWARE_VERSION = 0xFA,
  CHAIN_COMMAND_GET_DEVICE_TYPE = 0xFB,
  CHAIN_COMMAND_ENUMERATION_REQUEST = 0xFC,
  CHAIN_COMMAND_HEARTBEAT = 0xFD,
  CHAIN_COMMAND_ENUMERATION = 0xFE,
};

struct ChainFrame {
  uint8_t index{0};
  uint8_t command{0};
  const uint8_t *data{nullptr};
  uint16_t data_size{0};
};

class M5ChainBase : public Component, public uart::UARTDevice {
 public:
  ~M5ChainBase();

  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_uart_parent(uart::UARTComponent *parent);
  /// Position in the chain as seen from the controller (the first node is 1).
  void set_chain_index(uint8_t chain_index) { this->chain_index_ = chain_index; }
  uint8_t get_chain_index() const { return this->chain_index_; }
  void set_update_interval(uint32_t interval_ms) { this->update_interval_ms_ = interval_ms; }
  uint32_t get_update_interval() const { return this->update_interval_ms_; }
  bool is_update_interval_enabled() const { return this->update_interval_ms_ != SCHEDULER_DONT_RUN; }

  virtual ChainStatus set_rgb_color(uint8_t, uint8_t, uint8_t) { return ChainStatus::CHAIN_STATUS_NOT_INITIALIZED; }
  virtual ChainStatus set_rgb_color_component(uint8_t, uint8_t) { return ChainStatus::CHAIN_STATUS_NOT_INITIALIZED; }
  virtual ChainStatus set_led_brightness(uint8_t) { return ChainStatus::CHAIN_STATUS_NOT_INITIALIZED; }

  /// Encode and send one protocol frame without waiting for a response.
  bool send_packet(uint8_t index, uint8_t command, const uint8_t *data, uint16_t data_size);
  bool send_packet(uint8_t index, uint8_t command) { return this->send_packet(index, command, nullptr, 0); }
  bool send_packet(uint8_t index, ChainCommand command, const uint8_t *data, uint16_t data_size) {
    return this->send_packet(index, static_cast<uint8_t>(command), data, data_size);
  }
  bool send_packet(uint8_t index, ChainCommand command) {
    return this->send_packet(index, static_cast<uint8_t>(command), nullptr, 0);
  }

  /// Start one request. Only one request can be pending because the wire protocol has no sequence number.
  ChainStatus request(uint8_t index, uint8_t command, const uint8_t *data, uint16_t data_size,
                      uint32_t timeout_ms = 100);
  ChainStatus request(uint8_t index, uint8_t command, uint32_t timeout_ms = 100) {
    return this->request(index, command, nullptr, 0, timeout_ms);
  }
  ChainStatus request(uint8_t index, ChainCommand command, const uint8_t *data, uint16_t data_size,
                      uint32_t timeout_ms = 100) {
    return this->request(index, static_cast<uint8_t>(command), data, data_size, timeout_ms);
  }
  ChainStatus request(uint8_t index, ChainCommand command, uint32_t timeout_ms = 100) {
    return this->request(index, static_cast<uint8_t>(command), nullptr, 0, timeout_ms);
  }

  /// Whether this instance has a request on the wire.
  bool request_pending() const { return this->request_pending_; }
  uint8_t pending_index() const { return this->pending_index_; }
  uint8_t pending_command() const { return this->pending_command_; }

  static uint8_t calculate_crc(uint8_t index, uint8_t command, const uint8_t *data, uint16_t data_size);
  static uint8_t calculate_crc(const uint8_t *packet, uint16_t packet_size);
  static bool build_packet(uint8_t index, uint8_t command, const uint8_t *data, uint16_t data_size, uint8_t *packet,
                           uint16_t packet_capacity, uint16_t *packet_size);
  static bool validate_packet(const uint8_t *packet, uint16_t packet_size);

 protected:
  void dump_base_config();

  /// Called for a valid response matching the active request.
  virtual void on_response(const ChainFrame &) {}
  /// Called for valid frames that are not the active request, including unsolicited events.
  virtual void on_unsolicited(const ChainFrame &) {}
  /// Called when the active request expires without a matching response.
  virtual void on_request_timeout(uint8_t, uint8_t) {}

 private:
  void register_instance_();
  void unregister_instance_();
  void parse_buffer_();
  void dispatch_frame_(const uint8_t *packet, uint16_t packet_size);
  void dispatch_frame_to_instance_(const uint8_t *packet, uint16_t packet_size);
  void discard_prefix_(uint16_t count);
  void handle_timeout_(uint32_t now);

  std::array<uint8_t, MAX_PACKET_SIZE> receive_buffer_{};
  uint16_t receive_buffer_size_{0};
  std::array<uint8_t, MAX_PACKET_SIZE> send_buffer_{};

  bool request_pending_{false};
  uint8_t pending_index_{0};
  uint8_t pending_command_{0};
  uint32_t request_started_at_{0};
  uint32_t request_timeout_ms_{0};
  uint32_t update_interval_ms_{100};
  uint8_t chain_index_{1};
};

}  // namespace esphome::m5_chain_base
