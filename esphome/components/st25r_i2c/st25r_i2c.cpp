#include "st25r_i2c.h"

#include <cstring>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::st25r_i2c {

ESPHOME_LOG_TAG(TAG, "st25r_i2c");

// I2CBus::write_prefixed() only exists from ESPHome 2026.12 on, so join the operation bytes and
// the data here; writes up to 20 bytes stay on the stack.
static i2c::ErrorCode write_prefixed(i2c::I2CBus *bus, uint8_t address, const uint8_t *prefix, size_t prefix_len,
                                     const uint8_t *data, size_t len) {
  SmallBufferWithHeapFallback<20> buffer_alloc(prefix_len + len);
  uint8_t *buffer = buffer_alloc.get();
  if (prefix_len != 0)
    std::memcpy(buffer, prefix, prefix_len);
  if (len != 0)
    std::memcpy(buffer + prefix_len, data, len);
  return bus->write_readv(address, buffer, prefix_len + len, nullptr, 0);
}

// The I2C interface carries the same operation bytes as SPI. Bit 6 of the register address
// selects a read, 0xFB switches to space B, and the FIFO is reached through its own modes.
uint8_t ST25RI2c::read_register(uint8_t reg) {
  uint8_t prefix[2];
  size_t prefix_len = 0;
  if (reg & 0x40)
    prefix[prefix_len++] = 0xFB;  // Space B prefix
  prefix[prefix_len++] = 0x40 | (reg & 0x3F);

  uint8_t value{0};
  this->status_comm_result(this->write_read(prefix, prefix_len, &value, 1) == i2c::ERROR_OK);
  return value;
}

void ST25RI2c::write_register(uint8_t reg, uint8_t value) {
  uint8_t prefix[2];
  size_t prefix_len = 0;
  if (reg & 0x40)
    prefix[prefix_len++] = 0xFB;  // Space B prefix
  prefix[prefix_len++] = reg & 0x3F;

  this->status_comm_result(write_prefixed(this->bus_, this->address_, prefix, prefix_len, &value, 1) == i2c::ERROR_OK);
}

void ST25RI2c::write_command(uint8_t command) { this->status_comm_result(this->write(&command, 1) == i2c::ERROR_OK); }

void ST25RI2c::write_fifo(const uint8_t *data, size_t len) {
  const uint8_t op_code = 0x80;
  this->status_comm_result(write_prefixed(this->bus_, this->address_, &op_code, 1, data, len) == i2c::ERROR_OK);
}

void ST25RI2c::read_fifo(uint8_t *data, size_t len) {
  const uint8_t op_code = 0x9F;
  this->status_comm_result(this->write_read(&op_code, 1, data, len) == i2c::ERROR_OK);
}

void ST25RI2c::dump_config() {
  st25r::ST25R::dump_config();
  LOG_I2C_DEVICE(this)
}

}  // namespace esphome::st25r_i2c
