#pragma once

#include <algorithm>
#include <array>
#include "esphome/components/i2c/i2c_bus.h"

namespace esphome::m5_unit_ncir2::testing {

// The unit's STM32 firmware: a register byte alone selects a register, a register byte with data writes it, and a
// plain read returns the selected register. A read joined to a write by a repeated START, or longer than the
// register's field, locks the bus until a power cycle, as the hardware does.
class NCIR2Bus final : public i2c::I2CBus {
 public:
  std::array<uint8_t, 0x100> regs{};
  bool absent{false};
  bool locked{false};
  unsigned emissivity_writes{0};
  unsigned buzzer_writes{0};

  i2c::ErrorCode write_readv(uint8_t /*address*/, const uint8_t *out, size_t out_len, uint8_t *in,
                             size_t in_len) override {
    if (this->absent)
      return i2c::ERROR_NOT_ACKNOWLEDGED;
    if (this->locked)
      return i2c::ERROR_TIMEOUT;
    if (out_len != 0 && in_len != 0) {
      this->locked = true;
      return i2c::ERROR_TIMEOUT;
    }
    if (out_len != 0) {
      this->selected_ = out[0];
      std::copy(out + 1, out + out_len, this->regs.begin() + out[0]);
      this->emissivity_writes += out_len > 1 && out[0] == 0x10;
      this->buzzer_writes += out_len > 1 && out[0] == 0x50;
      return i2c::ERROR_OK;
    }
    if (in_len > field_length(this->selected_)) {
      this->locked = true;
      return i2c::ERROR_TIMEOUT;
    }
    std::copy_n(this->regs.begin() + this->selected_, in_len, in);
    return i2c::ERROR_OK;
  }

  void set_int16(uint8_t reg, int16_t value) {
    this->regs[reg] = static_cast<uint8_t>(value);
    this->regs[reg + 1] = static_cast<uint8_t>(static_cast<uint16_t>(value) >> 8);
  }
  uint16_t get_uint16(uint8_t reg) const { return this->regs[reg] | (this->regs[reg + 1] << 8); }

 protected:
  // Read lengths the vendor library uses
  static size_t field_length(uint8_t reg) {
    switch (reg) {
      case 0x00:
      case 0x10:
      case 0x20:
      case 0x22:
      case 0x40:
      case 0x42:
      case 0x45:
      case 0x47:
      case 0x50:
      case 0x90:
        return 2;
      case 0x30:
      case 0x33:
      case 0x60:
        return 3;
      default:
        return 1;
    }
  }

  uint8_t selected_{0};
};

}  // namespace esphome::m5_unit_ncir2::testing
