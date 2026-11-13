#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <thread>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "esphome/components/m5_chain_angle/m5_chain_angle.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::m5_chain_angle::testing {
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

class TestAngle : public M5ChainAngle {
 public:
  using M5ChainAngle::on_request_timeout;
};

static std::vector<uint8_t> packet(uint8_t index, uint8_t command, std::initializer_list<uint8_t> data = {}) {
  std::array<uint8_t, m5_chain_base::MAX_PACKET_SIZE> buffer{};
  uint16_t size = 0;
  EXPECT_TRUE(m5_chain_base::M5ChainBase::build_packet(index, command, data.begin(), data.size(), buffer.data(),
                                                       buffer.size(), &size));
  return {buffer.begin(), buffer.begin() + size};
}

static void wait_for_next_bus_slot() {
  std::this_thread::sleep_for(std::chrono::milliseconds(m5_chain_base::INTER_REQUEST_DELAY_MS + 2));
}

TEST(M5ChainAngleTest, PublishesAdc12AndAdc8ToTheirOwnSensors) {
  MockUART uart;
  M5ChainAngle angle;
  sensor::Sensor adc_12;
  sensor::Sensor adc_8;
  angle.set_uart_parent(&uart);
  angle.set_chain_index(6);
  angle.set_update_interval(0);
  angle.set_angle_sensor(AngleSensorType::ANGLE_SENSOR_ADC_12, &adc_12);
  angle.set_angle_sensor(AngleSensorType::ANGLE_SENSOR_ADC_8, &adc_8);

  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(2);
  angle.setup();
  angle.loop();
  uart.inject(packet(6, static_cast<uint8_t>(AngleCommand::ANGLE_COMMAND_GET_ADC_12), {0x34, 0x12}));
  angle.loop();

  EXPECT_FLOAT_EQ(adc_12.state, 0x1234);
  EXPECT_TRUE(std::isnan(adc_8.state));

  wait_for_next_bus_slot();
  angle.loop();
  uart.inject(packet(6, static_cast<uint8_t>(AngleCommand::ANGLE_COMMAND_GET_ADC_8), {0x56}));
  angle.loop();

  EXPECT_FLOAT_EQ(adc_12.state, 0x1234);
  EXPECT_FLOAT_EQ(adc_8.state, 0x56);
}

TEST(M5ChainAngleTest, AcceptsLateAdcResponseAsUnsolicited) {
  MockUART uart;
  M5ChainAngle angle;
  sensor::Sensor adc_12;
  angle.set_uart_parent(&uart);
  angle.set_chain_index(6);
  angle.set_angle_sensor(AngleSensorType::ANGLE_SENSOR_ADC_12, &adc_12);
  angle.setup();

  uart.inject(packet(6, static_cast<uint8_t>(AngleCommand::ANGLE_COMMAND_GET_ADC_12), {0x00, 0x40}));
  angle.loop();

  EXPECT_FLOAT_EQ(adc_12.state, 0x4000);
  EXPECT_FALSE(angle.status_has_warning());
}

TEST(M5ChainAngleTest, LateAdcResponseClearsTimeoutWarning) {
  MockUART uart;
  TestAngle angle;
  sensor::Sensor adc_12;
  angle.set_uart_parent(&uart);
  angle.set_chain_index(6);
  angle.set_angle_sensor(AngleSensorType::ANGLE_SENSOR_ADC_12, &adc_12);
  angle.setup();

  angle.on_request_timeout(6, static_cast<uint8_t>(AngleCommand::ANGLE_COMMAND_GET_ADC_12));
  EXPECT_TRUE(angle.status_has_warning());

  uart.inject(packet(6, static_cast<uint8_t>(AngleCommand::ANGLE_COMMAND_GET_ADC_12), {0x01, 0x00}));
  angle.loop();

  EXPECT_FLOAT_EQ(adc_12.state, 1);
  EXPECT_FALSE(angle.status_has_warning());
}

TEST(M5ChainAngleTest, CombinesRgbComponentsAcrossOutputUpdates) {
  MockUART uart;
  M5ChainAngle angle;
  angle.set_uart_parent(&uart);
  angle.set_chain_index(6);

  EXPECT_CALL(uart, write_array(::testing::_, 14)).Times(2);
  EXPECT_EQ(angle.set_rgb_color_component(0, 1), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  EXPECT_EQ(angle.set_rgb_color_component(1, 2), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  EXPECT_EQ(angle.set_rgb_color_component(2, 3), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);

  uart.inject(packet(6, static_cast<uint8_t>(m5_chain_base::ChainCommand::CHAIN_COMMAND_SET_RGB_VALUE), {1}));
  angle.loop();
  EXPECT_TRUE(angle.request_pending());
  wait_for_next_bus_slot();
  angle.loop();
}

}  // namespace
}  // namespace esphome::m5_chain_angle::testing
