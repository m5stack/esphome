#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esphome::scs9009 {

static constexpr uint8_t SCS_HEADER = 0xFF;
static constexpr uint8_t SCS_BROADCAST_ADDRESS = 0xFE;
static constexpr size_t SCS_MAX_PAYLOAD = 16;
static constexpr size_t SCS_MAX_SYNC_SERVOS = 8;
static constexpr size_t SCS_MAX_SYNC_DATA = 8;
static constexpr size_t SCS_MAX_SYNC_PARAMS = 2 + SCS_MAX_SYNC_SERVOS * (1 + SCS_MAX_SYNC_DATA);
// A response fits in SCS_MAX_PAYLOAD + 6; a sync request is larger, so size the buffers for it.
static constexpr size_t SCS_MAX_FRAME_SIZE = SCS_MAX_SYNC_PARAMS + 6;

// Instructions shared by the SCS protocol families
static constexpr uint8_t SCS_INSTRUCTION_PING = 0x01;
static constexpr uint8_t SCS_INSTRUCTION_READ = 0x02;
static constexpr uint8_t SCS_INSTRUCTION_WRITE = 0x03;
static constexpr uint8_t SCS_INSTRUCTION_REG_WRITE = 0x04;
static constexpr uint8_t SCS_INSTRUCTION_REG_ACTION = 0x05;
static constexpr uint8_t SCS_INSTRUCTION_SYNC_READ = 0x82;
static constexpr uint8_t SCS_INSTRUCTION_SYNC_WRITE = 0x83;

enum class SCSFrameResult : uint8_t {
  SCS_FRAME_RESULT_IN_PROGRESS,
  SCS_FRAME_RESULT_COMPLETE,
  SCS_FRAME_RESULT_INVALID,
};

/// A complete response frame: FF FF address length status data... checksum.
struct SCSFrame {
  uint8_t address{0};
  uint8_t status{0};
  uint8_t data[SCS_MAX_PAYLOAD]{};
  uint8_t data_len{0};
};

/// Build a request frame; returns the frame length, or 0 when the buffer is too small.
size_t scs_build_frame(uint8_t *buffer, size_t buffer_size, uint8_t address, uint8_t instruction, const uint8_t *params,
                       uint8_t params_len);

/// One servo entry of a SYNC WRITE request.
struct SCSSyncWriteEntry {
  uint8_t address;
  const uint8_t *data;
};

/// Build SYNC WRITE: a broadcast frame that writes the same register block of several servos.
size_t scs_build_sync_write(uint8_t *buffer, size_t buffer_size, uint8_t reg, uint8_t data_len,
                            const SCSSyncWriteEntry *entries, size_t count);

/// Build SYNC READ: a broadcast frame that asks several servos for the same register block.
size_t scs_build_sync_read(uint8_t *buffer, size_t buffer_size, uint8_t reg, uint8_t length, const uint8_t *addresses,
                           size_t count);

/// Incremental response parser; bytes before the FF FF header are skipped.
class SCSFrameParser {
 public:
  SCSFrameResult feed(uint8_t byte);

  /// The frame of the last SCS_FRAME_RESULT_COMPLETE, valid until the next feed().
  const SCSFrame &frame() const { return this->frame_; }

  /// The raw bytes of the last complete frame, for echo detection.
  const uint8_t *raw() const { return this->raw_; }
  size_t raw_length() const { return this->length_; }

  void reset();

 protected:
  uint8_t raw_[SCS_MAX_FRAME_SIZE]{};
  size_t index_{0};
  size_t expected_{0};
  size_t length_{0};
  SCSFrame frame_{};
};

inline void scs_encode_word(uint8_t *out, uint16_t value, bool big_endian) {
  if (big_endian) {
    out[0] = static_cast<uint8_t>(value >> 8);
    out[1] = static_cast<uint8_t>(value & 0xFF);
  } else {
    out[0] = static_cast<uint8_t>(value & 0xFF);
    out[1] = static_cast<uint8_t>(value >> 8);
  }
}

inline uint16_t scs_decode_word(const uint8_t *in, bool big_endian) {
  if (big_endian) {
    return static_cast<uint16_t>((in[0] << 8) | in[1]);
  }
  return static_cast<uint16_t>((in[1] << 8) | in[0]);
}

/// Sign-extend a value whose sign is stored in bit `sign_bit`.
inline int16_t scs_decode_signed(uint16_t value, uint8_t sign_bit) {
  const uint16_t mask = static_cast<uint16_t>(1 << sign_bit);
  if (value & mask) {
    return static_cast<int16_t>(-static_cast<int16_t>(value & ~mask));
  }
  return static_cast<int16_t>(value);
}

}  // namespace esphome::scs9009
