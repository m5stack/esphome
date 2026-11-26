#pragma once

#include <algorithm>
#include <array>
#include "esphome/components/i2c/i2c_bus.h"

namespace esphome::m5_unit_acssr::testing {

// The unit's ESP32 target: the first byte of a write selects a register and the rest write from there, and a read
// returns bytes from the selected register on. A restart clears every register.
class ACSSRBus final : public i2c::I2CBus {
 public:
  std::array<uint8_t, 0x100> regs{};
  bool absent{false};
  unsigned relay_writes{0};

  i2c::ErrorCode write_readv(uint8_t /*address*/, const uint8_t *out, size_t out_len, uint8_t *in,
                             size_t in_len) override {
    if (this->absent)
      return i2c::ERROR_NOT_ACKNOWLEDGED;
    if (out_len != 0) {
      this->selected_ = out[0];
      std::copy(out + 1, out + out_len, this->regs.begin() + out[0]);
      this->relay_writes += out_len > 1 && out[0] == 0x00;
    }
    std::copy_n(this->regs.begin() + this->selected_, in_len, in);
    return i2c::ERROR_OK;
  }

  void restart() {
    this->regs.fill(0);
    this->regs[0xFE] = 1;
  }

 protected:
  uint8_t selected_{0};
};

}  // namespace esphome::m5_unit_acssr::testing
