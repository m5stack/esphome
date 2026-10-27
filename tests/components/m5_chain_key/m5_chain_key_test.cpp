#include <algorithm>
#include <cstdint>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/m5_chain_key/m5_chain_key.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::m5_chain_key::testing {
namespace {

static constexpr uint8_t KEY_STATUS_COMMAND = 0xE1;

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

TEST(M5ChainKeyTest, PublishesButtonStatusFromBaseParser) {
  MockUART uart;
  M5ChainKey key;
  binary_sensor::BinarySensor sensor;
  key.set_uart_parent(&uart);
  key.set_chain_index(7);
  key.set_key_sensor(&sensor);

  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(1);
  key.setup();
  uart.inject(packet(7, KEY_STATUS_COMMAND, {1}));
  key.loop();

  EXPECT_TRUE(sensor.get_state());
}

TEST(M5ChainKeyTest, DoesNotPollWhenBinarySensorIsNotConfigured) {
  MockUART uart;
  M5ChainKey key;
  key.set_uart_parent(&uart);

  EXPECT_CALL(uart, write_array(::testing::_, ::testing::_)).Times(0);
  key.setup();
}

TEST(M5ChainKeyTest, AcceptsLateButtonStatusAsUnsolicitedResponse) {
  MockUART uart;
  M5ChainKey key;
  binary_sensor::BinarySensor sensor;
  key.set_uart_parent(&uart);
  key.set_key_sensor(&sensor);

  // No request is pending here, so this exercises the late-response path directly.
  uart.inject(packet(1, KEY_STATUS_COMMAND, {1}));
  key.loop();

  EXPECT_TRUE(sensor.get_state());
  EXPECT_FALSE(key.status_has_warning());
}

TEST(M5ChainKeyTest, UsesBaseRequestForRgbCommands) {
  MockUART uart;
  M5ChainKey key;
  key.set_uart_parent(&uart);
  key.set_chain_index(4);

  EXPECT_CALL(uart, write_array(::testing::_, 14)).Times(1);
  EXPECT_EQ(key.set_rgb_color(1, 2, 3), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  EXPECT_TRUE(key.request_pending());
}

TEST(M5ChainKeyTest, CombinesRgbComponentsAcrossOutputUpdates) {
  MockUART uart;
  M5ChainKey key;
  key.set_uart_parent(&uart);
  key.set_chain_index(4);

  EXPECT_CALL(uart, write_array(::testing::_, 14)).Times(2);
  EXPECT_EQ(key.set_rgb_color_component(0, 1), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  EXPECT_TRUE(key.request_pending());
  EXPECT_EQ(key.set_rgb_color_component(1, 2), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  EXPECT_TRUE(key.request_pending());
  EXPECT_EQ(key.set_rgb_color_component(2, 3), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  EXPECT_TRUE(key.request_pending());

  uart.inject(packet(4, static_cast<uint8_t>(m5_chain_base::ChainCommand::CHAIN_COMMAND_SET_RGB_VALUE), {1}));
  key.loop();
  EXPECT_TRUE(key.request_pending());
}

}  // namespace
}  // namespace esphome::m5_chain_key::testing
