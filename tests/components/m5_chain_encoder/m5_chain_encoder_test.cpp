#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "esphome/components/m5_chain_encoder/m5_chain_encoder.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::m5_chain_encoder::testing {
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

static std::vector<uint8_t> packet(uint8_t index, uint8_t command, std::initializer_list<uint8_t> data = {}) {
  std::array<uint8_t, m5_chain_base::MAX_PACKET_SIZE> buffer{};
  uint16_t size = 0;
  EXPECT_TRUE(m5_chain_base::M5ChainBase::build_packet(index, command, data.begin(), data.size(), buffer.data(),
                                                       buffer.size(), &size));
  return {buffer.begin(), buffer.begin() + size};
}

TEST(M5ChainEncoderTest, PublishesSignedLittleEndianValues) {
  MockUART uart;
  M5ChainEncoder encoder;
  sensor::Sensor value;
  sensor::Sensor increment;
  encoder.set_uart_parent(&uart);
  encoder.set_chain_index(6);
  encoder.set_encoder_sensor(EncoderSensorType::ENCODER_SENSOR_VALUE, &value);
  encoder.set_encoder_sensor(EncoderSensorType::ENCODER_SENSOR_INCREMENT, &increment);

  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(3);
  encoder.setup();
  uart.inject(packet(6, static_cast<uint8_t>(EncoderCommand::ENCODER_COMMAND_GET_VALUE), {0xFE, 0xFF}));
  encoder.loop();
  EXPECT_FLOAT_EQ(value.state, -2);

  uart.inject(packet(6, static_cast<uint8_t>(EncoderCommand::ENCODER_COMMAND_GET_INCREMENT), {0x34, 0x12}));
  encoder.loop();
  EXPECT_FLOAT_EQ(increment.state, 0x1234);
}

TEST(M5ChainEncoderTest, RoutesRequestsToConfiguredChainIndex) {
  MockUART uart;
  M5ChainEncoder encoder;
  sensor::Sensor direction;
  encoder.set_uart_parent(&uart);
  encoder.set_chain_index(9);
  encoder.set_encoder_sensor(EncoderSensorType::ENCODER_SENSOR_AB_STATUS, &direction);

  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(2);
  encoder.setup();
  uart.inject(packet(9, static_cast<uint8_t>(EncoderCommand::ENCODER_COMMAND_GET_AB_STATUS), {1}));
  encoder.loop();
  EXPECT_FLOAT_EQ(direction.state, 1);
}

TEST(M5ChainEncoderTest, UsesSharedRgbOutputApi) {
  MockUART uart;
  M5ChainEncoder encoder;
  encoder.set_uart_parent(&uart);
  encoder.set_chain_index(3);

  EXPECT_CALL(uart, write_array(::testing::_, 14)).Times(1);
  EXPECT_EQ(encoder.set_rgb_color(1, 2, 3), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  EXPECT_TRUE(encoder.request_pending());
}

TEST(M5ChainEncoderTest, FlushesRgbUpdateAfterPollingResponse) {
  MockUART uart;
  M5ChainEncoder encoder;
  sensor::Sensor value;
  encoder.set_uart_parent(&uart);
  encoder.set_chain_index(4);
  encoder.set_encoder_sensor(EncoderSensorType::ENCODER_SENSOR_VALUE, &value);

  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(1);
  EXPECT_CALL(uart, write_array(::testing::_, 14)).Times(1);
  encoder.setup();

  // The output update arrives while the encoder poll request is outstanding.
  EXPECT_EQ(encoder.set_rgb_color_component(0, 10), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  EXPECT_EQ(encoder.set_rgb_color_component(1, 20), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  EXPECT_EQ(encoder.set_rgb_color_component(2, 30), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  EXPECT_TRUE(encoder.request_pending());

  uart.inject(packet(4, static_cast<uint8_t>(EncoderCommand::ENCODER_COMMAND_GET_VALUE), {0x34, 0x12}));
  encoder.loop();

  EXPECT_TRUE(encoder.request_pending());
}

}  // namespace
}  // namespace esphome::m5_chain_encoder::testing
