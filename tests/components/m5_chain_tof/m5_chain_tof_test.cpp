#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "esphome/components/m5_chain_tof/m5_chain_tof.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::m5_chain_tof::testing {
namespace {

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

class TestToF : public M5ChainToF {
 public:
  using M5ChainToF::on_request_timeout;
};

static std::vector<uint8_t> packet(uint8_t index, uint8_t command, std::initializer_list<uint8_t> data = {}) {
  std::array<uint8_t, m5_chain_base::MAX_PACKET_SIZE> buffer{};
  uint16_t size = 0;
  EXPECT_TRUE(m5_chain_base::M5ChainBase::build_packet(index, command, data.begin(), data.size(), buffer.data(),
                                                       buffer.size(), &size));
  return {buffer.begin(), buffer.begin() + size};
}

TEST(M5ChainToFTest, PublishesLittleEndianDistance) {
  MockUART uart;
  M5ChainToF tof;
  sensor::Sensor distance;
  tof.set_uart_parent(&uart);
  tof.set_chain_index(7);
  tof.set_distance_sensor(&distance);

  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(1);
  tof.setup();
  uart.inject(packet(7, static_cast<uint8_t>(TofCommand::TOF_COMMAND_GET_DISTANCE), {0x34, 0x12}));
  tof.loop();

  EXPECT_FLOAT_EQ(distance.state, 0x1234);
}

TEST(M5ChainToFTest, AcceptsLateDistanceAsUnsolicitedResponse) {
  MockUART uart;
  M5ChainToF tof;
  sensor::Sensor distance;
  tof.set_uart_parent(&uart);
  tof.set_chain_index(7);
  tof.set_distance_sensor(&distance);

  uart.inject(packet(7, static_cast<uint8_t>(TofCommand::TOF_COMMAND_GET_DISTANCE), {0x78, 0x56}));
  tof.loop();

  EXPECT_FLOAT_EQ(distance.state, 0x5678);
  EXPECT_FALSE(tof.status_has_warning());
}

TEST(M5ChainToFTest, LateDistanceClearsTimeoutWarning) {
  MockUART uart;
  TestToF tof;
  sensor::Sensor distance;
  tof.set_uart_parent(&uart);
  tof.set_chain_index(7);
  tof.set_distance_sensor(&distance);

  tof.on_request_timeout(7, static_cast<uint8_t>(TofCommand::TOF_COMMAND_GET_DISTANCE));
  EXPECT_TRUE(tof.status_has_warning());

  uart.inject(packet(7, static_cast<uint8_t>(TofCommand::TOF_COMMAND_GET_DISTANCE), {0x01, 0x00}));
  tof.loop();

  EXPECT_FLOAT_EQ(distance.state, 1);
  EXPECT_FALSE(tof.status_has_warning());
}

TEST(M5ChainToFTest, CombinesRgbComponentsAcrossOutputUpdates) {
  MockUART uart;
  M5ChainToF tof;
  tof.set_uart_parent(&uart);
  tof.set_chain_index(4);

  EXPECT_CALL(uart, write_array(::testing::_, 14)).Times(2);
  EXPECT_EQ(tof.set_rgb_color_component(0, 1), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  EXPECT_EQ(tof.set_rgb_color_component(1, 2), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  EXPECT_EQ(tof.set_rgb_color_component(2, 3), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);

  uart.inject(packet(4, static_cast<uint8_t>(m5_chain_base::ChainCommand::CHAIN_COMMAND_SET_RGB_VALUE), {1}));
  tof.loop();
  EXPECT_TRUE(tof.request_pending());
}

}  // namespace
}  // namespace esphome::m5_chain_tof::testing
