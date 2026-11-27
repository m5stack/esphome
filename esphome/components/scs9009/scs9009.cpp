#include "scs9009.h"

#include <array>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include "scs9009_servo.h"

namespace esphome::scs9009 {

ESPHOME_LOG_TAG(TAG, "scs9009");

bool SCS9009Component::queue_read(SCS9009Servo *servo, uint8_t address, uint8_t reg, uint8_t length,
                                  bool expect_response) {
  Request request;
  request.servo = servo;
  request.address = address;
  request.instruction = SCS_INSTRUCTION_READ;
  request.reg = reg;
  request.length = length;
  request.expect_response = expect_response;
  return this->enqueue_(request);
}

bool SCS9009Component::queue_write(SCS9009Servo *servo, uint8_t address, uint8_t reg, const uint8_t *data,
                                   uint8_t length, bool expect_response) {
  if (length > MAX_WRITE_PAYLOAD) {
    ESP_LOGW(TAG, "Write of %u bytes does not fit the request queue", length);
    return false;
  }
  Request request;
  request.servo = servo;
  request.address = address;
  request.instruction = SCS_INSTRUCTION_WRITE;
  request.reg = reg;
  request.length = length;
  request.expect_response = expect_response;
  std::memcpy(request.data, data, length);
  return this->enqueue_(request);
}

bool SCS9009Component::enqueue_(const Request &request) {
  if (this->pending_.size() >= MAX_PENDING) {
    ESP_LOGW(TAG, "Request queue full, dropping a command for address %u", request.address);
    return false;
  }
  this->pending_.push(request);
  return true;
}

bool SCS9009Component::begin_sync_write() {
  if (this->sync_write_active_) {
    return false;
  }
  this->sync_write_active_ = true;
  this->sync_write_.count = 0;
  this->sync_write_.reg = 0;
  this->sync_write_.data_len = 0;
  return true;
}

bool SCS9009Component::add_sync_write(SCS9009Servo *servo, uint8_t address, uint8_t reg, const uint8_t *data,
                                      uint8_t length) {
  if (!this->sync_write_active_ || servo == nullptr || data == nullptr || length == 0 || length > SCS_MAX_SYNC_DATA ||
      this->sync_write_.count >= SCS_MAX_SYNC_SERVOS) {
    return false;
  }
  if (this->sync_write_.count == 0) {
    this->sync_write_.reg = reg;
    this->sync_write_.data_len = length;
  } else if (reg != this->sync_write_.reg || length != this->sync_write_.data_len) {
    return false;
  }

  SyncWriteEntry &entry = this->sync_write_.entries[this->sync_write_.count++];
  entry.servo = servo;
  entry.address = address;
  std::memcpy(entry.data, data, length);
  return true;
}

bool SCS9009Component::end_sync_write() {
  if (!this->sync_write_active_ || this->sync_write_.count == 0) {
    this->sync_write_active_ = false;
    return false;
  }
  this->sync_write_active_ = false;

  Request request;
  request.instruction = SCS_INSTRUCTION_SYNC_WRITE;
  request.sync = true;
  if (!this->enqueue_(request)) {
    this->sync_write_.count = 0;
    return false;
  }
  return true;
}

void SCS9009Component::abort_sync_write() {
  this->sync_write_active_ = false;
  this->sync_write_.count = 0;
}

bool SCS9009Component::queue_sync_read(SCS9009Servo *const *servos, uint8_t count, uint8_t reg, uint8_t length) {
  if (servos == nullptr || count == 0 || count > SCS_MAX_SYNC_SERVOS || length == 0 || length > SCS_MAX_PAYLOAD ||
      this->sync_read_queued_) {
    return false;
  }
  for (uint8_t i = 0; i < count; i++) {
    if (servos[i] == nullptr) {
      return false;
    }
    for (uint8_t j = 0; j < i; j++) {
      if (servos[i]->address() == servos[j]->address()) {
        ESP_LOGW(TAG, "Duplicate address %u in a sync read", servos[i]->address());
        return false;
      }
    }
  }

  this->sync_read_.reg = reg;
  this->sync_read_.length = length;
  this->sync_read_.count = count;
  this->sync_read_.received = 0;
  for (uint8_t i = 0; i < count; i++) {
    this->sync_read_.servos[i] = servos[i];
  }

  Request request;
  request.instruction = SCS_INSTRUCTION_SYNC_READ;
  request.sync = true;
  if (!this->enqueue_(request)) {
    this->sync_read_.count = 0;
    return false;
  }
  this->sync_read_queued_ = true;
  return true;
}

void SCS9009Component::loop() {
  const uint32_t now = App.get_loop_component_start_time();

  if (this->waiting_response_) {
    while (this->available() > 0) {
      uint8_t byte;
      if (!this->read_byte(&byte)) {
        break;
      }
      const SCSFrameResult result = this->parser_.feed(byte);
      if (result == SCSFrameResult::SCS_FRAME_RESULT_COMPLETE) {
        if (this->is_echo_()) {
          this->parser_.reset();
          continue;
        }
        if (this->handle_frame_(now)) {
          this->parser_.reset();
          continue;
        }
        return;
      }
      if (result == SCSFrameResult::SCS_FRAME_RESULT_INVALID) {
        // A bad header or a failed checksum is not the end of the transaction: the rest of
        // the response may still be arriving. Keep the parser aligned and let the response
        // timeout decide, so one stray byte does not throw away a whole frame.
        this->parser_.reset();
        continue;
      }
    }
    if (now - this->sent_ms_ > this->timeout_ms_) {
      this->discard_available();
      if (this->active_.sync) {
        this->finish_sync_read_(false, ftservo::SERVO_ERROR_NO_REPLY, now);
      } else {
        this->finish_request_(false, ftservo::SERVO_ERROR_NO_REPLY, now);
      }
    }
    return;
  }

  if (this->request_active_ || this->pending_.empty()) {
    return;
  }
  if (now - this->last_frame_ms_ < this->min_interval_ms_) {
    return;
  }

  this->active_ = this->pending_.front();
  this->pending_.pop();
  this->request_active_ = true;
  this->start_request_(now);
}

void SCS9009Component::start_request_(uint32_t now) {
  if (this->active_.sync) {
    if (this->active_.instruction == SCS_INSTRUCTION_SYNC_WRITE) {
      this->start_sync_write_(now);
    } else {
      this->start_sync_read_(now);
    }
    return;
  }

  uint8_t params[MAX_WRITE_PAYLOAD + 2];
  uint8_t params_len;
  if (this->active_.instruction == SCS_INSTRUCTION_READ) {
    params[0] = this->active_.reg;
    params[1] = this->active_.length;
    params_len = 2;
  } else {
    params[0] = this->active_.reg;
    std::memcpy(&params[1], this->active_.data, this->active_.length);
    params_len = static_cast<uint8_t>(this->active_.length + 1);
  }

  const size_t frame_len = scs_build_frame(this->tx_frame_, sizeof(this->tx_frame_), this->active_.address,
                                           this->active_.instruction, params, params_len);
  if (frame_len == 0) {
    ESP_LOGW(TAG, "Failed to build a frame for address %u", this->active_.address);
    this->finish_request_(false, ftservo::SERVO_ERROR_LENGTH, now);
    return;
  }

  this->tx_length_ = static_cast<uint8_t>(frame_len);
  this->discard_rx_();
  this->write_array(this->tx_frame_, frame_len);
  const uart::UARTFlushResult flush_result = this->flush();
  if (flush_result == uart::UARTFlushResult::UART_FLUSH_RESULT_TIMEOUT ||
      flush_result == uart::UARTFlushResult::UART_FLUSH_RESULT_FAILED) {
    this->finish_request_(false, ftservo::SERVO_ERROR_NO_REPLY, now);
    return;
  }

  if (!this->active_.expect_response) {
    this->finish_request_(true, ftservo::SERVO_ERROR_NONE, now);
    return;
  }

  this->parser_.reset();
  this->sent_ms_ = now;
  this->waiting_response_ = true;
}

void SCS9009Component::start_sync_write_(uint32_t now) {
  std::array<SCSSyncWriteEntry, SCS_MAX_SYNC_SERVOS> entries{};
  for (uint8_t i = 0; i < this->sync_write_.count; i++) {
    entries[i].address = this->sync_write_.entries[i].address;
    entries[i].data = this->sync_write_.entries[i].data;
  }

  const size_t frame_len = scs_build_sync_write(this->tx_frame_, sizeof(this->tx_frame_), this->sync_write_.reg,
                                                this->sync_write_.data_len, entries.data(), this->sync_write_.count);
  if (frame_len == 0) {
    this->finish_sync_write_(false, ftservo::SERVO_ERROR_LENGTH, now);
    return;
  }

  this->tx_length_ = static_cast<uint8_t>(frame_len);
  this->discard_rx_();
  this->write_array(this->tx_frame_, frame_len);
  const uart::UARTFlushResult flush_result = this->flush();
  if (flush_result == uart::UARTFlushResult::UART_FLUSH_RESULT_TIMEOUT ||
      flush_result == uart::UARTFlushResult::UART_FLUSH_RESULT_FAILED) {
    this->finish_sync_write_(false, ftservo::SERVO_ERROR_NO_REPLY, now);
    return;
  }

  this->finish_sync_write_(true, ftservo::SERVO_ERROR_NONE, now);
}

void SCS9009Component::start_sync_read_(uint32_t now) {
  uint8_t params[2 + SCS_MAX_SYNC_SERVOS];
  params[0] = this->sync_read_.reg;
  params[1] = this->sync_read_.length;
  for (uint8_t i = 0; i < this->sync_read_.count; i++) {
    params[2 + i] = this->sync_read_.servos[i]->address();
  }

  const size_t frame_len =
      scs_build_frame(this->tx_frame_, sizeof(this->tx_frame_), SCS_BROADCAST_ADDRESS, SCS_INSTRUCTION_SYNC_READ,
                      params, static_cast<uint8_t>(this->sync_read_.count + 2));
  if (frame_len == 0) {
    this->finish_sync_read_(false, ftservo::SERVO_ERROR_LENGTH, now);
    return;
  }

  this->tx_length_ = static_cast<uint8_t>(frame_len);
  this->discard_rx_();
  this->write_array(this->tx_frame_, frame_len);
  const uart::UARTFlushResult flush_result = this->flush();
  if (flush_result == uart::UARTFlushResult::UART_FLUSH_RESULT_TIMEOUT ||
      flush_result == uart::UARTFlushResult::UART_FLUSH_RESULT_FAILED) {
    this->finish_sync_read_(false, ftservo::SERVO_ERROR_NO_REPLY, now);
    return;
  }

  this->parser_.reset();
  this->sent_ms_ = now;
  this->waiting_response_ = true;
}

bool SCS9009Component::handle_frame_(uint32_t now) {
  if (this->active_.sync) {
    return this->handle_sync_read_frame_(now);
  }

  const SCSFrame &frame = this->parser_.frame();
  if (frame.address != this->active_.address) {
    this->finish_request_(false, ftservo::SERVO_ERROR_ADDRESS, now);
    return false;
  }

  const bool is_read = this->active_.instruction == SCS_INSTRUCTION_READ;
  if (is_read ? frame.data_len != this->active_.length : frame.data_len != 0) {
    this->finish_request_(false, ftservo::SERVO_ERROR_LENGTH, now);
    return false;
  }

  SCS9009Servo *servo = this->active_.servo;
  const uint8_t reg = this->active_.reg;
  this->waiting_response_ = false;
  this->request_active_ = false;
  this->last_frame_ms_ = now;
  if (servo == nullptr) {
    return false;
  }
  if (is_read) {
    servo->on_scs_read(reg, frame.data, frame.data_len, frame.status);
  } else {
    servo->on_scs_write(true, ftservo::SERVO_ERROR_NONE);
  }
  return false;
}

bool SCS9009Component::handle_sync_read_frame_(uint32_t now) {
  if (this->sync_read_.received >= this->sync_read_.count) {
    this->finish_sync_read_(false, ftservo::SERVO_ERROR_LENGTH, now);
    return false;
  }

  SCS9009Servo *servo = this->sync_read_.servos[this->sync_read_.received];
  const SCSFrame &frame = this->parser_.frame();
  if (servo == nullptr || frame.address != servo->address()) {
    this->finish_sync_read_(false, ftservo::SERVO_ERROR_ADDRESS, now);
    return false;
  }
  if (frame.data_len != this->sync_read_.length) {
    this->finish_sync_read_(false, ftservo::SERVO_ERROR_LENGTH, now);
    return false;
  }

  servo->on_scs_read(this->sync_read_.reg, frame.data, frame.data_len, frame.status);
  this->sync_read_.received++;
  if (this->sync_read_.received >= this->sync_read_.count) {
    this->finish_sync_read_(true, ftservo::SERVO_ERROR_NONE, now);
    return false;
  }

  this->sent_ms_ = now;
  return true;
}

void SCS9009Component::finish_sync_write_(bool ok, uint8_t error, uint32_t now) {
  this->request_active_ = false;
  this->waiting_response_ = false;
  this->last_frame_ms_ = now;
  for (uint8_t i = 0; i < this->sync_write_.count; i++) {
    if (this->sync_write_.entries[i].servo != nullptr) {
      this->sync_write_.entries[i].servo->on_scs_write(ok, error);
    }
  }
  this->sync_write_.count = 0;
}

void SCS9009Component::finish_sync_read_(bool ok, uint8_t error, uint32_t now) {
  if (!ok) {
    for (uint8_t i = this->sync_read_.received; i < this->sync_read_.count; i++) {
      if (this->sync_read_.servos[i] != nullptr) {
        this->sync_read_.servos[i]->on_scs_read_failed(error);
      }
    }
  }
  this->sync_read_.count = 0;
  this->sync_read_.received = 0;
  this->sync_read_queued_ = false;
  this->waiting_response_ = false;
  this->request_active_ = false;
  this->last_frame_ms_ = now;
}

void SCS9009Component::finish_request_(bool ok, uint8_t error, uint32_t now) {
  SCS9009Servo *servo = this->active_.servo;
  const bool is_read = this->active_.instruction == SCS_INSTRUCTION_READ;
  this->waiting_response_ = false;
  this->request_active_ = false;
  this->last_frame_ms_ = now;
  if (servo == nullptr) {
    return;
  }
  if (is_read) {
    servo->on_scs_read_failed(error);
  } else {
    servo->on_scs_write(ok, error);
  }
}

void SCS9009Component::discard_rx_() {
  // The servos answer one request at a time, so whatever is still in the receive buffer
  // belongs to an earlier frame: a transceiver echo, or a response that arrived after its
  // timeout. Keeping it would make the next response parse out of step, which is what turns
  // one missed frame into a run of checksum and length errors. The reference driver drains
  // the buffer the same way before every request.
  this->discard_available();
}

bool SCS9009Component::is_echo_() const {
  return this->tx_length_ > 0 && this->parser_.raw_length() == this->tx_length_ &&
         std::memcmp(this->parser_.raw(), this->tx_frame_, this->tx_length_) == 0;
}

void SCS9009Component::dump_config() {
  ESP_LOGCONFIG(TAG, "SCS9009 bus:");
  ESP_LOGCONFIG(TAG, "  Response timeout: %u ms, minimum interval: %u ms", this->timeout_ms_, this->min_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Requests waiting: %u, sync write/read: up to %u servos", this->pending_.size(),
                static_cast<unsigned>(SCS_MAX_SYNC_SERVOS));
}

}  // namespace esphome::scs9009
