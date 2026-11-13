#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/m5_chain_pir/m5_chain_pir.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::m5_chain_pir::testing {
namespace {

static constexpr uint8_t PIR_STATUS_COMMAND = 0x37;
static constexpr uint8_t PIR_EVENT_REPORT_COMMAND = 0xE0;
static constexpr uint8_t PIR_SET_EVENT_REPORT_COMMAND = 0xE1;
static constexpr uint8_t PIR_SET_KEEP_SECONDS_COMMAND = 0xE3;
static constexpr uint8_t PIR_EVENT_REPORT_TYPE = 0x05;

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

TEST(M5ChainPirTest, PublishesPresenceFromPolledStatus) {
  MockUART uart;
  M5ChainPir pir;
  binary_sensor::BinarySensor sensor;
  pir.set_uart_parent(&uart);
  pir.set_chain_index(2);
  pir.set_update_interval(0);
  pir.set_presence_sensor(&sensor);

  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(1);
  pir.setup();
  pir.loop();
  uart.inject(packet(2, PIR_STATUS_COMMAND, {1}));
  pir.loop();

  EXPECT_TRUE(sensor.get_state());
  EXPECT_FALSE(pir.status_has_warning());
}

TEST(M5ChainPirTest, PublishesPresenceFromEventReport) {
  MockUART uart;
  M5ChainPir pir;
  binary_sensor::BinarySensor sensor;
  pir.set_uart_parent(&uart);
  pir.set_chain_index(2);
  pir.set_presence_sensor(&sensor);
  pir.setup();

  uart.inject(packet(2, PIR_EVENT_REPORT_COMMAND, {1, PIR_EVENT_REPORT_TYPE}));
  pir.loop();
  EXPECT_TRUE(sensor.get_state());

  uart.inject(packet(2, PIR_EVENT_REPORT_COMMAND, {0, PIR_EVENT_REPORT_TYPE}));
  pir.loop();
  EXPECT_FALSE(sensor.get_state());
  EXPECT_FALSE(pir.status_has_warning());
}

TEST(M5ChainPirTest, ReportsWarningForEventReportWithoutDeviceTypeCode) {
  MockUART uart;
  M5ChainPir pir;
  binary_sensor::BinarySensor sensor;
  pir.set_uart_parent(&uart);
  pir.set_chain_index(2);
  pir.set_presence_sensor(&sensor);
  pir.setup();

  uart.inject(packet(2, PIR_EVENT_REPORT_COMMAND, {1, 0x04}));
  pir.loop();

  EXPECT_FALSE(sensor.has_state());
  EXPECT_TRUE(pir.status_has_warning());
}

TEST(M5ChainPirTest, IgnoresEventReportOfOtherChainNodes) {
  MockUART uart;
  M5ChainPir pir;
  binary_sensor::BinarySensor sensor;
  pir.set_uart_parent(&uart);
  pir.set_chain_index(2);
  pir.set_presence_sensor(&sensor);
  pir.setup();

  uart.inject(packet(3, PIR_EVENT_REPORT_COMMAND, {1, PIR_EVENT_REPORT_TYPE}));
  pir.loop();

  EXPECT_FALSE(sensor.has_state());
}

TEST(M5ChainPirTest, DoesNotPollWhenBinarySensorIsNotConfigured) {
  MockUART uart;
  M5ChainPir pir;
  pir.set_uart_parent(&uart);
  pir.set_update_interval(0);

  EXPECT_CALL(uart, write_array(::testing::_, ::testing::_)).Times(0);
  pir.setup();
  pir.loop();
}

TEST(M5ChainPirTest, SendsEventReportModeRequest) {
  MockUART uart;
  M5ChainPir pir;
  pir.set_uart_parent(&uart);
  pir.set_chain_index(4);

  EXPECT_CALL(uart, write_array(::testing::_, 10)).Times(1);
  EXPECT_EQ(pir.set_event_report(true), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  EXPECT_TRUE(pir.request_pending());

  uart.inject(packet(4, PIR_SET_EVENT_REPORT_COMMAND, {1}));
  pir.loop();
  EXPECT_FALSE(pir.request_pending());
}

TEST(M5ChainPirTest, SendsKeepSecondsRequestWithFlashFlag) {
  MockUART uart;
  M5ChainPir pir;
  pir.set_uart_parent(&uart);
  pir.set_chain_index(4);

  EXPECT_CALL(uart, write_array(::testing::_, 11)).Times(1);
  EXPECT_EQ(pir.set_keep_seconds(10, true), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  EXPECT_TRUE(pir.request_pending());

  uart.inject(packet(4, PIR_SET_KEEP_SECONDS_COMMAND, {1}));
  pir.loop();
  EXPECT_FALSE(pir.request_pending());
  EXPECT_FALSE(pir.status_has_warning());
}

TEST(M5ChainPirTest, UsesBaseRequestForRgbCommands) {
  MockUART uart;
  M5ChainPir pir;
  pir.set_uart_parent(&uart);
  pir.set_chain_index(4);

  EXPECT_CALL(uart, write_array(::testing::_, 14)).Times(1);
  EXPECT_EQ(pir.set_rgb_color(1, 2, 3), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  EXPECT_TRUE(pir.request_pending());
}

}  // namespace
}  // namespace esphome::m5_chain_pir::testing
