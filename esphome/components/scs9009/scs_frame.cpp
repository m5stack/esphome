#include "scs_frame.h"

namespace esphome::scs9009 {

size_t scs_build_frame(uint8_t *buffer, size_t buffer_size, uint8_t address, uint8_t instruction, const uint8_t *params,
                       uint8_t params_len) {
  if (params_len > 253) {
    return 0;
  }
  const size_t total = static_cast<size_t>(params_len) + 6;
  if (total > buffer_size) {
    return 0;
  }

  buffer[0] = SCS_HEADER;
  buffer[1] = SCS_HEADER;
  buffer[2] = address;
  buffer[3] = static_cast<uint8_t>(params_len + 2);
  buffer[4] = instruction;

  uint8_t checksum = static_cast<uint8_t>(address + buffer[3] + instruction);
  for (uint8_t i = 0; i < params_len; i++) {
    buffer[5 + i] = params[i];
    checksum = static_cast<uint8_t>(checksum + params[i]);
  }
  buffer[total - 1] = static_cast<uint8_t>(~checksum);
  return total;
}

size_t scs_build_sync_write(uint8_t *buffer, size_t buffer_size, uint8_t reg, uint8_t data_len,
                            const SCSSyncWriteEntry *entries, size_t count) {
  if (entries == nullptr || count == 0 || count > SCS_MAX_SYNC_SERVOS || data_len == 0 ||
      data_len > SCS_MAX_SYNC_DATA) {
    return 0;
  }

  std::array<uint8_t, SCS_MAX_SYNC_PARAMS> params{};
  size_t params_len = 0;
  params[params_len++] = reg;
  params[params_len++] = data_len;
  for (size_t i = 0; i < count; i++) {
    if (entries[i].data == nullptr) {
      return 0;
    }
    params[params_len++] = entries[i].address;
    std::memcpy(&params[params_len], entries[i].data, data_len);
    params_len += data_len;
  }
  return scs_build_frame(buffer, buffer_size, SCS_BROADCAST_ADDRESS, SCS_INSTRUCTION_SYNC_WRITE, params.data(),
                         static_cast<uint8_t>(params_len));
}

size_t scs_build_sync_read(uint8_t *buffer, size_t buffer_size, uint8_t reg, uint8_t length, const uint8_t *addresses,
                           size_t count) {
  if (addresses == nullptr || count == 0 || count > SCS_MAX_SYNC_SERVOS || length == 0 || length > SCS_MAX_PAYLOAD) {
    return 0;
  }

  std::array<uint8_t, 2 + SCS_MAX_SYNC_SERVOS> params{};
  params[0] = reg;
  params[1] = length;
  std::memcpy(&params[2], addresses, count);
  return scs_build_frame(buffer, buffer_size, SCS_BROADCAST_ADDRESS, SCS_INSTRUCTION_SYNC_READ, params.data(),
                         static_cast<uint8_t>(count + 2));
}

SCSFrameResult SCSFrameParser::feed(uint8_t byte) {
  if (this->index_ < 2) {
    if (byte == SCS_HEADER) {
      this->raw_[this->index_++] = byte;
    } else {
      this->index_ = 0;
    }
    return SCSFrameResult::SCS_FRAME_RESULT_IN_PROGRESS;
  }

  // 0xFF is not a valid address: a run of 0xFF bytes is still the header, so the last two win.
  if (this->index_ == 2 && byte == SCS_HEADER) {
    return SCSFrameResult::SCS_FRAME_RESULT_IN_PROGRESS;
  }
  if (this->index_ >= SCS_MAX_FRAME_SIZE) {
    this->reset();
    return SCSFrameResult::SCS_FRAME_RESULT_INVALID;
  }
  this->raw_[this->index_++] = byte;

  if (this->index_ < 4) {
    return SCSFrameResult::SCS_FRAME_RESULT_IN_PROGRESS;
  }
  if (this->index_ == 4) {
    const uint8_t length = this->raw_[3];
    if (length < 2 || static_cast<size_t>(length - 2) > SCS_MAX_PAYLOAD) {
      this->reset();
      return SCSFrameResult::SCS_FRAME_RESULT_INVALID;
    }
    this->expected_ = static_cast<size_t>(length) + 4;
    if (this->expected_ > SCS_MAX_FRAME_SIZE) {
      this->reset();
      return SCSFrameResult::SCS_FRAME_RESULT_INVALID;
    }
  }
  if (this->index_ < this->expected_) {
    return SCSFrameResult::SCS_FRAME_RESULT_IN_PROGRESS;
  }

  uint8_t checksum = 0;
  for (size_t i = 2; i + 1 < this->expected_; i++) {
    checksum = static_cast<uint8_t>(checksum + this->raw_[i]);
  }
  if (static_cast<uint8_t>(~checksum) != this->raw_[this->expected_ - 1]) {
    this->reset();
    return SCSFrameResult::SCS_FRAME_RESULT_INVALID;
  }

  this->length_ = this->expected_;
  this->frame_.address = this->raw_[2];
  this->frame_.status = this->raw_[4];
  this->frame_.data_len = static_cast<uint8_t>(this->raw_[3] - 2);
  std::memcpy(this->frame_.data, &this->raw_[5], this->frame_.data_len);

  this->index_ = 0;
  this->expected_ = 0;
  return SCSFrameResult::SCS_FRAME_RESULT_COMPLETE;
}

void SCSFrameParser::reset() {
  this->index_ = 0;
  this->expected_ = 0;
}

}  // namespace esphome::scs9009
