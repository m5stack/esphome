// Shared fixture for the AXP2101 tests. The entity tests live in the platform
// subdirectories so the test harness pulls in those platform components.

#pragma once

#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>

#include "esphome/components/axp2101/axp2101.h"

namespace esphome::axp2101::testing {

constexpr uint8_t ADDRESS = 0x34;

/// Register file behind a fake I2C bus.
class RegisterBus final : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                             size_t read_count) override {
    if (address != ADDRESS || this->failure != i2c::ERROR_OK)
      return this->failure;
    if (write_count == 0)
      return i2c::ERROR_OK;

    const uint8_t reg = write_buffer[0];
    if (read_count > 0) {
      for (size_t i = 0; i < read_count; i++)
        read_buffer[i] = this->read(static_cast<uint8_t>(reg + i));
      return i2c::ERROR_OK;
    }
    this->writes++;
    for (size_t i = 1; i < write_count; i++) {
      const uint8_t target = static_cast<uint8_t>(reg + i - 1);
      if (this->write_one_to_clear.count(target) != 0) {
        this->registers[target] &= static_cast<uint8_t>(~write_buffer[i]);
      } else {
        this->registers[target] = write_buffer[i];
      }
    }
    return i2c::ERROR_OK;
  }

  uint8_t read(uint8_t reg) const {
    auto it = this->registers.find(reg);
    return it == this->registers.end() ? 0 : it->second;
  }
  void poke(uint8_t reg, uint8_t value) { this->registers[reg] = value; }

  std::map<uint8_t, uint8_t> registers;
  /// How many writes the component made, for tests that must not write at all.
  size_t writes{0};
  /// Registers that clear a bit when a one is written to it, like the interrupt
  /// status registers do.
  std::set<uint8_t> write_one_to_clear{0x48, 0x49, 0x4A};
  i2c::ErrorCode failure{i2c::ERROR_OK};
};

class Axp2101Test : public ::testing::Test {
 protected:
  void SetUp() override {
    this->axp_.set_i2c_bus(&this->bus_);
    this->axp_.set_i2c_address(ADDRESS);
    this->bus_.poke(0x03, 0x4A);
  }

  const axp_base::AxpRail *rail(const char *name) { return this->axp_.find_rail(name); }

  const axp_base::AxpAdcChannel *channel(const char *name) {
    for (uint8_t i = 0; i < this->axp_.get_model().adc_count; i++) {
      if (std::string(this->axp_.get_model().adc_channels[i].name) == name)
        return &this->axp_.get_model().adc_channels[i];
    }
    return nullptr;
  }

  RegisterBus bus_;
  AXP2101 axp_;
};

}  // namespace esphome::axp2101::testing
