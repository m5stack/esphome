#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/st25r/st25r.h"

namespace esphome::st25r_i2c {

class ST25RI2c final : public st25r::ST25R, public i2c::I2CDevice {
 public:
  void dump_config() override;

 protected:
  uint8_t read_register(uint8_t reg) override;
  void write_register(uint8_t reg, uint8_t value) override;
  void write_command(uint8_t command) override;
  void write_fifo(const uint8_t *data, size_t len) override;
  void read_fifo(uint8_t *data, size_t len) override;
};

}  // namespace esphome::st25r_i2c
