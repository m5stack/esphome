#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "scs_frame.h"

namespace esphome::scs9009 {

class SCS9009Servo;

/**
 * The SCS bus. It serializes one request at a time, writes it with the FT-SCS frame format and
 * parses the response without blocking. Servo models implement the register access and call
 * queue_read()/queue_write(); the bus reports the outcome back to the servo.
 */
class SCS9009Component : public uart::UARTDevice, public Component {
 public:
  void loop() override;
  void dump_config() override;

  /// Longest wait for a response frame.
  void set_timeout(uint32_t timeout_ms) { this->timeout_ms_ = timeout_ms; }
  /// Minimum silence between two frames, as the servos need time to accept back-to-back commands.
  void set_min_interval(uint32_t min_interval_ms) { this->min_interval_ms_ = min_interval_ms; }

  bool queue_read(SCS9009Servo *servo, uint8_t address, uint8_t reg, uint8_t length, bool expect_response);
  bool queue_write(SCS9009Servo *servo, uint8_t address, uint8_t reg, const uint8_t *data, uint8_t length,
                   bool expect_response);

  /**
   * SYNC WRITE: collect one goal per servo, then send a single broadcast frame that makes all of
   * them start together. Every entry must use the same register and data length.
   */
  bool begin_sync_write();
  bool add_sync_write(SCS9009Servo *servo, uint8_t address, uint8_t reg, const uint8_t *data, uint8_t length);
  bool end_sync_write();
  void abort_sync_write();
  bool sync_write_active() const { return this->sync_write_active_; }

  /**
   * SYNC READ: one broadcast request, one response per servo in the requested order. Responses are
   * dispatched to each servo's on_scs_read() as they arrive.
   */
  bool queue_sync_read(SCS9009Servo *const *servos, uint8_t count, uint8_t reg, uint8_t length);

 protected:
  static constexpr size_t MAX_PENDING = 8;
  static constexpr size_t MAX_WRITE_PAYLOAD = 8;

  struct Request {
    SCS9009Servo *servo{nullptr};
    uint8_t address{0};
    uint8_t instruction{0};
    uint8_t reg{0};
    uint8_t length{0};
    bool expect_response{false};
    bool sync{false};
    uint8_t data[MAX_WRITE_PAYLOAD]{};
  };

  struct SyncWriteEntry {
    SCS9009Servo *servo{nullptr};
    uint8_t address{0};
    uint8_t data[SCS_MAX_SYNC_DATA]{};
  };

  struct SyncWrite {
    uint8_t reg{0};
    uint8_t data_len{0};
    uint8_t count{0};
    SyncWriteEntry entries[SCS_MAX_SYNC_SERVOS]{};
  };

  struct SyncRead {
    SCS9009Servo *servos[SCS_MAX_SYNC_SERVOS]{};
    uint8_t reg{0};
    uint8_t length{0};
    uint8_t count{0};
    uint8_t received{0};
  };

  bool enqueue_(const Request &request);
  void start_request_(uint32_t now);
  bool handle_frame_(uint32_t now);
  bool handle_sync_read_frame_(uint32_t now);
  void start_sync_write_(uint32_t now);
  void start_sync_read_(uint32_t now);
  void finish_request_(bool ok, uint8_t error, uint32_t now);
  void finish_sync_write_(bool ok, uint8_t error, uint32_t now);
  void finish_sync_read_(bool ok, uint8_t error, uint32_t now);
  bool is_echo_() const;

  StaticRingBuffer<Request, MAX_PENDING> pending_{};
  Request active_{};
  SyncWrite sync_write_{};
  SyncRead sync_read_{};
  SCSFrameParser parser_{};
  uint8_t tx_frame_[SCS_MAX_FRAME_SIZE]{};
  uint8_t tx_length_{0};
  uint32_t timeout_ms_{10};
  uint32_t min_interval_ms_{2};
  uint32_t sent_ms_{0};
  uint32_t last_frame_ms_{0};
  bool request_active_{false};
  bool waiting_response_{false};
  bool sync_write_active_{false};
  bool sync_write_queued_{false};
  bool sync_read_queued_{false};
};

}  // namespace esphome::scs9009
