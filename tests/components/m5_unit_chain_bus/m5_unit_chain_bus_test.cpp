#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "esphome/components/m5_unit_chain_bus/m5_unit_chain_bus.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/base_automation.h"

namespace esphome::m5_unit_chain_bus::testing {
namespace {

static constexpr uint8_t I2C_READ_COMMAND = 0x11;
static constexpr uint8_t I2C_SCAN_COMMAND = 0x15;
static constexpr uint8_t SET_OUTPUT_LEVEL_COMMAND = 0x31;
static constexpr uint8_t GET_INPUT_LEVEL_COMMAND = 0x41;
static constexpr uint8_t GET_ADC_VALUE_COMMAND = 0x61;
static constexpr uint8_t GET_WORK_STATUS_COMMAND = 0x70;
static constexpr uint8_t NVIC_TRIGGER_REPORT_COMMAND = 0xE0;

class MockUART : public uart::UARTComponent {
 public:
  void inject(const std::vector<uint8_t> &bytes) { this->rx_.insert(this->rx_.end(), bytes.begin(), bytes.end()); }
  size_t available() override { return this->rx_.size() - this->position_; }
  bool read_array(uint8_t *data, size_t size) override {
    if (size > this->available())
      return false;
    std::copy_n(this->rx_.data() + this->position_, size, data);
    this->position_ += size;
    return true;
  }
  bool peek_byte(uint8_t *data) override {
    if (!this->available())
      return false;
    *data = this->rx_[this->position_];
    return true;
  }
  MOCK_METHOD(void, write_array, (const uint8_t *data, size_t size), (override));
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS; }
  void check_logger_conflict() override {}
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool) override {}
#endif

 protected:
  std::vector<uint8_t> rx_;
  size_t position_{0};
};

static std::vector<uint8_t> packet(uint8_t index, uint8_t command, std::initializer_list<uint8_t> data = {}) {
  std::array<uint8_t, m5_chain_base::MAX_PACKET_SIZE> buffer{};
  uint16_t size = 0;
  EXPECT_TRUE(m5_chain_base::M5ChainBase::build_packet(index, command, data.begin(), data.size(), buffer.data(),
                                                       buffer.size(), &size));
  return {buffer.begin(), buffer.begin() + size};
}

TEST(M5UnitChainBusTest, PollsWorkStatusAndClearsWarning) {
  MockUART uart;
  M5UnitChainBus bus;
  bus.set_uart_parent(&uart);
  bus.set_chain_index(4);
  bus.set_update_interval(0);
  bus.setup();

  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(1);
  bus.loop();
  uart.inject(packet(4, GET_WORK_STATUS_COMMAND, {5, 1}));
  bus.loop();

  EXPECT_FALSE(bus.status_has_warning());
}

TEST(M5UnitChainBusTest, FiresI2cReadCallbackWithDeviceAddress) {
  MockUART uart;
  M5UnitChainBus bus;
  uint8_t read_address = 0;
  std::vector<uint8_t> read_data;
  bus.set_uart_parent(&uart);
  bus.set_chain_index(4);
  bus.add_on_i2c_read_callback([&](uint8_t address, std::vector<uint8_t> data) {
    read_address = address;
    read_data = std::move(data);
  });

  EXPECT_CALL(uart, write_array(::testing::_, 11)).Times(1);
  EXPECT_EQ(bus.i2c_read(0x50, 2), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  uart.inject(packet(4, I2C_READ_COMMAND, {1, 0xAA, 0xBB}));
  bus.loop();

  EXPECT_EQ(read_address, 0x50);
  EXPECT_EQ(read_data, std::vector<uint8_t>({0xAA, 0xBB}));
  EXPECT_FALSE(bus.status_has_warning());
}

TEST(M5UnitChainBusTest, DoesNotFireI2cReadCallbackWhenOperationFails) {
  MockUART uart;
  M5UnitChainBus bus;
  bool called = false;
  bus.set_uart_parent(&uart);
  bus.set_chain_index(4);
  bus.add_on_i2c_read_callback([&](uint8_t, std::vector<uint8_t>) { called = true; });

  EXPECT_CALL(uart, write_array(::testing::_, ::testing::_)).Times(1);
  EXPECT_EQ(bus.i2c_read(0x50, 1), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  uart.inject(packet(4, I2C_READ_COMMAND, {0}));
  bus.loop();

  EXPECT_FALSE(called);
  EXPECT_TRUE(bus.status_has_warning());
}

TEST(M5UnitChainBusTest, FiresI2cScanCallbackWithFoundAddresses) {
  MockUART uart;
  M5UnitChainBus bus;
  std::vector<uint8_t> addresses;
  bus.set_uart_parent(&uart);
  bus.set_chain_index(4);
  bus.add_on_i2c_scan_callback([&](std::vector<uint8_t> found) { addresses = std::move(found); });

  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(1);
  EXPECT_EQ(bus.i2c_scan(), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  uart.inject(packet(4, I2C_SCAN_COMMAND, {1, 2, 0x50, 0x68}));
  bus.loop();

  EXPECT_EQ(addresses, std::vector<uint8_t>({0x50, 0x68}));
}

TEST(M5UnitChainBusTest, FiresPinLevelCallbackForInputRead) {
  MockUART uart;
  M5UnitChainBus bus;
  uint8_t pin = 0;
  bool level = false;
  bool called = false;
  bus.set_uart_parent(&uart);
  bus.set_chain_index(4);
  bus.add_on_pin_level_callback([&](uint8_t callback_pin, bool callback_level) {
    pin = callback_pin;
    level = callback_level;
    called = true;
  });

  EXPECT_CALL(uart, write_array(::testing::_, 10)).Times(1);
  EXPECT_EQ(bus.get_input_level(2), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  uart.inject(packet(4, GET_INPUT_LEVEL_COMMAND, {1, 1}));
  bus.loop();

  EXPECT_TRUE(called);
  EXPECT_EQ(pin, 2);
  EXPECT_TRUE(level);
}

TEST(M5UnitChainBusTest, FiresAdcValueCallbackForAdcRead) {
  MockUART uart;
  M5UnitChainBus bus;
  uint16_t value = 0;
  bus.set_uart_parent(&uart);
  bus.set_chain_index(4);
  bus.add_on_adc_value_callback([&](uint8_t, uint16_t callback_value) { value = callback_value; });

  EXPECT_CALL(uart, write_array(::testing::_, 10)).Times(1);
  EXPECT_EQ(bus.get_adc_value(1), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  // 0x0ABC little endian.
  uart.inject(packet(4, GET_ADC_VALUE_COMMAND, {1, 0xBC, 0x0A}));
  bus.loop();

  EXPECT_EQ(value, 0x0ABC);
}

TEST(M5UnitChainBusTest, ReportsFallingEdgeAsLowLevel) {
  MockUART uart;
  M5UnitChainBus bus;
  uint8_t pin = 0;
  bool level = true;
  bus.set_uart_parent(&uart);
  bus.set_chain_index(4);
  bus.add_on_pin_level_callback([&](uint8_t callback_pin, bool callback_level) {
    pin = callback_pin;
    level = callback_level;
  });
  bus.setup();

  // Trigger mode 1 is a falling edge, so the pin is now low.
  uart.inject(packet(4, NVIC_TRIGGER_REPORT_COMMAND, {1, 2}));
  bus.loop();

  EXPECT_EQ(pin, 2);
  EXPECT_FALSE(level);
}

TEST(M5UnitChainBusTest, RejectsInvalidPin) {
  MockUART uart;
  M5UnitChainBus bus;
  bus.set_uart_parent(&uart);
  bus.set_chain_index(4);

  EXPECT_CALL(uart, write_array(::testing::_, ::testing::_)).Times(0);
  EXPECT_EQ(bus.set_output_level(0, true), m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR);
  EXPECT_EQ(bus.set_input_mode(3, ChainBusPull::CHAIN_BUS_PULL_UP),
            m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR);
  EXPECT_EQ(bus.get_adc_value(0), m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR);
  EXPECT_EQ(bus.set_adc_mode(3), m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR);
}

TEST(M5UnitChainBusTest, RejectsI2cTransferOutsideSupportedLength) {
  MockUART uart;
  M5UnitChainBus bus;
  bus.set_uart_parent(&uart);
  bus.set_chain_index(4);

  EXPECT_CALL(uart, write_array(::testing::_, ::testing::_)).Times(0);
  EXPECT_EQ(bus.i2c_read(0x50, 0), m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR);
  EXPECT_EQ(bus.i2c_read(0x50, 65), m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR);
  EXPECT_EQ(bus.i2c_write(0x50, {}), m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR);
}

TEST(M5UnitChainBusTest, RunsAutomationRegisteredThroughTriggerForwarder) {
  MockUART uart;
  M5UnitChainBus bus;
  bus.set_uart_parent(&uart);
  bus.set_chain_index(4);

  // Code generation registers a TriggerForwarder rather than a plain lambda, so mirror that here.
  Automation<uint8_t, std::vector<uint8_t>> automation;
  std::vector<uint8_t> received;
  LambdaAction<uint8_t, std::vector<uint8_t>> action(
      [&received](uint8_t, std::vector<uint8_t> data) { received = std::move(data); });
  automation.add_action(&action);
  bus.add_on_i2c_read_callback(TriggerForwarder<uint8_t, std::vector<uint8_t>>{&automation});

  EXPECT_CALL(uart, write_array(::testing::_, 11)).Times(1);
  EXPECT_EQ(bus.i2c_read(0x50, 2), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  uart.inject(packet(4, I2C_READ_COMMAND, {1, 0x11, 0x22}));
  bus.loop();

  EXPECT_EQ(received, std::vector<uint8_t>({0x11, 0x22}));
}

TEST(M5UnitChainBusTest, SendsOutputLevelRequest) {
  MockUART uart;
  M5UnitChainBus bus;
  bus.set_uart_parent(&uart);
  bus.set_chain_index(4);

  EXPECT_CALL(uart, write_array(::testing::_, 11)).Times(1);
  EXPECT_EQ(bus.set_output_level(1, true), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  EXPECT_TRUE(bus.request_pending());
}

}  // namespace
}  // namespace esphome::m5_unit_chain_bus::testing
