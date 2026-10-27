#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/m5_chain_joystick/m5_chain_joystick.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::m5_chain_joystick::testing {
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

static std::vector<uint8_t> packet(uint8_t index, uint8_t command, const std::vector<uint8_t> &data) {
  std::array<uint8_t, m5_chain_base::MAX_PACKET_SIZE> buffer{};
  uint16_t size = 0;
  EXPECT_TRUE(m5_chain_base::M5ChainBase::build_packet(index, command, data.data(), data.size(), buffer.data(),
                                                       buffer.size(), &size));
  return {buffer.begin(), buffer.begin() + size};
}

TEST(M5ChainJoystickTest, PublishesRawAndMappedValues) {
  MockUART uart;
  M5ChainJoystick joystick;
  sensor::Sensor raw_x;
  sensor::Sensor raw_y;
  sensor::Sensor mapped_x;
  joystick.set_uart_parent(&uart);
  joystick.set_chain_index(5);
  joystick.set_joystick_sensor(JoystickSensorType::JOYSTICK_SENSOR_ADC_16, 0, 0, &raw_x);
  joystick.set_joystick_sensor(JoystickSensorType::JOYSTICK_SENSOR_ADC_16, 1, 0, &raw_y);
  joystick.set_joystick_sensor(JoystickSensorType::JOYSTICK_SENSOR_MAPPED_16, 0, 0, &mapped_x);

  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(3);
  joystick.setup();
  uart.inject(packet(5, static_cast<uint8_t>(JoystickCommand::JOYSTICK_COMMAND_GET_ADC_16), {0x34, 0x12, 0xCD, 0xAB}));
  joystick.loop();
  EXPECT_FLOAT_EQ(raw_x.state, 0x1234);
  EXPECT_FLOAT_EQ(raw_y.state, 0xABCD);

  uart.inject(
      packet(5, static_cast<uint8_t>(JoystickCommand::JOYSTICK_COMMAND_GET_MAPPED_16), {0xFE, 0xFF, 0x02, 0x80}));
  joystick.loop();
  EXPECT_FLOAT_EQ(mapped_x.state, -2);
}

TEST(M5ChainJoystickTest, PublishesEightBitAndMappingRangeValues) {
  MockUART uart;
  M5ChainJoystick joystick;
  sensor::Sensor adc_x;
  sensor::Sensor mapped_y;
  sensor::Sensor range;
  joystick.set_uart_parent(&uart);
  joystick.set_chain_index(8);
  joystick.set_joystick_sensor(JoystickSensorType::JOYSTICK_SENSOR_ADC_8, 0, 0, &adc_x);
  joystick.set_joystick_sensor(JoystickSensorType::JOYSTICK_SENSOR_MAPPED_8, 1, 0, &mapped_y);
  joystick.set_joystick_sensor(JoystickSensorType::JOYSTICK_SENSOR_MAPPING_RANGE, 0, 3, &range);

  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(4);
  joystick.setup();
  uart.inject(packet(8, static_cast<uint8_t>(JoystickCommand::JOYSTICK_COMMAND_GET_ADC_8), {200, 150}));
  joystick.loop();
  EXPECT_FLOAT_EQ(adc_x.state, 200);

  uart.inject(packet(8, static_cast<uint8_t>(JoystickCommand::JOYSTICK_COMMAND_GET_MAPPED_8), {1, 0x80}));
  joystick.loop();
  EXPECT_FLOAT_EQ(mapped_y.state, -128);

  std::vector<uint8_t> range_data(16, 0);
  range_data[6] = 0x78;
  range_data[7] = 0x56;
  uart.inject(packet(8, static_cast<uint8_t>(JoystickCommand::JOYSTICK_COMMAND_GET_MAPPING_RANGE), range_data));
  joystick.loop();
  EXPECT_FLOAT_EQ(range.state, 0x5678);
}

TEST(M5ChainJoystickTest, PublishesButtonAndUsesSharedRgbApi) {
  MockUART uart;
  M5ChainJoystick joystick;
  binary_sensor::BinarySensor button;
  joystick.set_uart_parent(&uart);
  joystick.set_chain_index(11);
  joystick.set_button_sensor(&button);

  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(2);
  joystick.setup();
  uart.inject(packet(11, static_cast<uint8_t>(JoystickCommand::JOYSTICK_COMMAND_GET_BUTTON_STATUS), {1}));
  joystick.loop();
  EXPECT_TRUE(button.get_state());

  joystick.set_button_sensor(nullptr);
  uart.inject(packet(11, static_cast<uint8_t>(JoystickCommand::JOYSTICK_COMMAND_GET_BUTTON_STATUS), {1}));
  joystick.loop();
  EXPECT_CALL(uart, write_array(::testing::_, 14)).Times(1);
  EXPECT_EQ(joystick.set_rgb_color(4, 5, 6), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
}

TEST(M5ChainJoystickTest, FlushesRgbUpdateAfterPollingResponse) {
  MockUART uart;
  M5ChainJoystick joystick;
  sensor::Sensor adc_x;
  joystick.set_uart_parent(&uart);
  joystick.set_chain_index(3);
  joystick.set_joystick_sensor(JoystickSensorType::JOYSTICK_SENSOR_ADC_16, 0, 0, &adc_x);

  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(1);
  EXPECT_CALL(uart, write_array(::testing::_, 14)).Times(1);
  joystick.setup();

  // The output update arrives while the joystick poll request is outstanding.
  EXPECT_EQ(joystick.set_rgb_color_component(0, 10), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  EXPECT_EQ(joystick.set_rgb_color_component(1, 20), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  EXPECT_EQ(joystick.set_rgb_color_component(2, 30), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  EXPECT_TRUE(joystick.request_pending());

  uart.inject(packet(3, static_cast<uint8_t>(JoystickCommand::JOYSTICK_COMMAND_GET_ADC_16), {0x34, 0x12, 0x78, 0x56}));
  joystick.loop();

  EXPECT_TRUE(joystick.request_pending());
}

}  // namespace
}  // namespace esphome::m5_chain_joystick::testing
