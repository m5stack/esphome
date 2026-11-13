#pragma once

#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

#include "esphome/components/m5_chain_base/m5_chain_base.h"
#include "esphome/core/helpers.h"

namespace esphome::m5_unit_chain_bus {

/// Longest I2C transfer the firmware accepts.
static constexpr uint8_t CHAIN_BUS_I2C_MAX_LENGTH = 64;

enum class ChainBusCommand : uint8_t {
  CHAIN_BUS_COMMAND_I2C_INIT = 0x10,
  CHAIN_BUS_COMMAND_I2C_READ = 0x11,
  CHAIN_BUS_COMMAND_I2C_WRITE = 0x12,
  CHAIN_BUS_COMMAND_I2C_MEM_READ = 0x13,
  CHAIN_BUS_COMMAND_I2C_MEM_WRITE = 0x14,
  CHAIN_BUS_COMMAND_I2C_SCAN_ADDRESS = 0x15,
  CHAIN_BUS_COMMAND_SET_OUTPUT_MODE = 0x30,
  CHAIN_BUS_COMMAND_SET_OUTPUT_LEVEL = 0x31,
  CHAIN_BUS_COMMAND_GET_OUTPUT_LEVEL = 0x32,
  CHAIN_BUS_COMMAND_SET_INPUT_MODE = 0x40,
  CHAIN_BUS_COMMAND_GET_INPUT_LEVEL = 0x41,
  CHAIN_BUS_COMMAND_SET_NVIC_MODE = 0x50,
  CHAIN_BUS_COMMAND_SET_ADC_MODE = 0x60,
  CHAIN_BUS_COMMAND_GET_ADC_VALUE = 0x61,
  CHAIN_BUS_COMMAND_GET_WORK_STATUS = 0x70,
  CHAIN_BUS_COMMAND_NVIC_TRIGGER_REPORT = 0xE0,
};

enum class ChainBusI2CSpeed : uint8_t {
  CHAIN_BUS_I2C_SPEED_100_KHZ = 0x00,
  CHAIN_BUS_I2C_SPEED_400_KHZ = 0x01,
};

enum class ChainBusRegisterSize : uint8_t {
  CHAIN_BUS_REGISTER_SIZE_8_BIT = 0x01,
  CHAIN_BUS_REGISTER_SIZE_16_BIT = 0x02,
};

enum class ChainBusOutputMode : uint8_t {
  CHAIN_BUS_OUTPUT_MODE_PUSH_PULL = 0x00,
  CHAIN_BUS_OUTPUT_MODE_OPEN_DRAIN = 0x01,
};

enum class ChainBusPull : uint8_t {
  CHAIN_BUS_PULL_UP = 0x00,
  CHAIN_BUS_PULL_DOWN = 0x01,
  CHAIN_BUS_PULL_NONE = 0x02,
};

enum class ChainBusTrigger : uint8_t {
  CHAIN_BUS_TRIGGER_RISING = 0x00,
  CHAIN_BUS_TRIGGER_FALLING = 0x01,
  CHAIN_BUS_TRIGGER_RISING_FALLING = 0x02,
};

/// The Unit ChainBus bridges its two Grove pins to I2C, GPIO or ADC, one mode at a time.
class M5UnitChainBus : public m5_chain_base::M5ChainBase {
 public:
  void setup() override;
  void dump_config() override;

  /// Results of an I2C read, in the byte order the device returned them.
  template<typename F> void add_on_i2c_read_callback(F &&callback) {
    this->i2c_read_callback_.add(std::forward<F>(callback));
  }
  /// Addresses that answered the last I2C scan.
  template<typename F> void add_on_i2c_scan_callback(F &&callback) {
    this->i2c_scan_callback_.add(std::forward<F>(callback));
  }
  /// Pin level read back from an input or output pin, and every external interrupt report.
  template<typename F> void add_on_pin_level_callback(F &&callback) {
    this->pin_level_callback_.add(std::forward<F>(callback));
  }
  /// ADC value of a pin in ADC mode, 0 to 4095.
  template<typename F> void add_on_adc_value_callback(F &&callback) {
    this->adc_value_callback_.add(std::forward<F>(callback));
  }

  m5_chain_base::ChainStatus i2c_init(ChainBusI2CSpeed speed);
  m5_chain_base::ChainStatus i2c_write(uint8_t address, std::initializer_list<uint8_t> data);
  m5_chain_base::ChainStatus i2c_read(uint8_t address, uint8_t length);
  m5_chain_base::ChainStatus i2c_mem_read(uint8_t address, uint16_t reg, ChainBusRegisterSize reg_size, uint8_t length);
  m5_chain_base::ChainStatus i2c_mem_write(uint8_t address, uint16_t reg, ChainBusRegisterSize reg_size,
                                           std::initializer_list<uint8_t> data);
  m5_chain_base::ChainStatus i2c_scan();

  m5_chain_base::ChainStatus set_output_mode(uint8_t pin, ChainBusOutputMode mode, ChainBusPull pull);
  m5_chain_base::ChainStatus set_output_level(uint8_t pin, bool level);
  m5_chain_base::ChainStatus get_output_level(uint8_t pin);
  m5_chain_base::ChainStatus set_input_mode(uint8_t pin, ChainBusPull pull);
  m5_chain_base::ChainStatus get_input_level(uint8_t pin);
  m5_chain_base::ChainStatus set_nvic_mode(uint8_t pin, ChainBusPull pull, ChainBusTrigger trigger);
  m5_chain_base::ChainStatus set_adc_mode(uint8_t pin);
  m5_chain_base::ChainStatus get_adc_value(uint8_t pin);

  m5_chain_base::ChainStatus set_rgb_color(uint8_t red, uint8_t green, uint8_t blue) override;
  m5_chain_base::ChainStatus set_rgb_color_component(uint8_t component, uint8_t value) override;
  m5_chain_base::ChainStatus set_led_brightness(uint8_t brightness) override;

 protected:
  void on_response(const m5_chain_base::ChainFrame &frame) override;
  void on_unsolicited(const m5_chain_base::ChainFrame &frame) override;
  void on_request_timeout(uint8_t index, uint8_t command) override;

  void poll_next_() override;
  void handle_i2c_read_(const m5_chain_base::ChainFrame &frame);
  void handle_i2c_scan_(const m5_chain_base::ChainFrame &frame);
  void handle_pin_level_(const m5_chain_base::ChainFrame &frame);
  void handle_adc_value_(const m5_chain_base::ChainFrame &frame);
  void handle_work_status_(const m5_chain_base::ChainFrame &frame);
  /// Log a warning and return false when the frame is missing a successful operation status.
  bool operation_succeeded_(const m5_chain_base::ChainFrame &frame);
  m5_chain_base::ChainStatus send_pending_rgb_();

  /// Address of the outstanding I2C read; the response does not echo it.
  uint8_t read_address_{0};
  /// Pin of the outstanding pin or ADC read; the response does not echo it.
  uint8_t read_pin_{0};

  LazyCallbackManager<void(uint8_t, std::vector<uint8_t>)> i2c_read_callback_;
  LazyCallbackManager<void(std::vector<uint8_t>)> i2c_scan_callback_;
  LazyCallbackManager<void(uint8_t, bool)> pin_level_callback_;
  LazyCallbackManager<void(uint8_t, uint16_t)> adc_value_callback_;

  uint8_t rgb_red_{0};
  uint8_t rgb_green_{0};
  uint8_t rgb_blue_{0};
  bool rgb_update_pending_{false};
  bool timeout_warning_active_{false};
};

}  // namespace esphome::m5_unit_chain_bus
