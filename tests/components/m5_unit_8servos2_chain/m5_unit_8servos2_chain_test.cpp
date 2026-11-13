#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "esphome/components/m5_unit_8servos2_chain/m5_unit_8servos2_chain.h"
#include "esphome/components/m5_unit_8servos2_chain/light/m5_unit_8servos2_chain_light.h"
#include "esphome/components/m5_unit_8servos2_chain/number/m5_unit_8servos2_chain_number.h"
#include "esphome/components/m5_unit_8servos2_chain/output/m5_unit_8servos2_chain_output.h"
#include "esphome/components/m5_unit_8servos2_chain/switch/m5_unit_8servos2_chain_switch.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::m5_unit_8servos2_chain::testing {
namespace {

static constexpr uint8_t GET_MODE_ALL_COMMAND = 0x13;
static constexpr uint8_t SET_MODE_COMMAND = 0x10;
static constexpr uint8_t SET_OUTPUT_LEVEL_ALL_COMMAND = 0x41;
static constexpr uint8_t SET_SERVO_ANGLE_COMMAND = 0x60;
static constexpr uint8_t SET_SERVO_ANGLE_ALL_COMMAND = 0x61;
static constexpr uint8_t SET_RGB_CONFIG_COMMAND = 0x70;
static constexpr uint8_t SET_RGB_BUFFER_COMMAND = 0x74;
/// Offset of the first data byte in a protocol frame.
static constexpr size_t DATA_OFFSET = 6;

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

/// Answer the pending request, then let the paced bus start the frame the flush queued after it.
static void deliver(M5Unit8Servos2Chain &servos, MockUART &uart, uint8_t command,
                    std::initializer_list<uint8_t> data = {1}) {
  uart.inject(packet(5, command, data));
  servos.loop();
  std::this_thread::sleep_for(std::chrono::milliseconds(m5_chain_base::INTER_REQUEST_DELAY_MS + 2));
  servos.loop();
}

TEST(M5Unit8Servos2ChainTest, PollsChannelModesAndClearsWarning) {
  MockUART uart;
  M5Unit8Servos2Chain servos;
  servos.set_uart_parent(&uart);
  servos.set_chain_index(5);
  servos.set_update_interval(0);
  servos.setup();

  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(1);
  servos.loop();
  uart.inject(packet(5, GET_MODE_ALL_COMMAND, {3, 3, 3, 3, 3, 3, 3, 3}));
  servos.loop();

  EXPECT_FALSE(servos.status_has_warning());
}

TEST(M5Unit8Servos2ChainTest, ReportsWarningForShortModeResponse) {
  MockUART uart;
  M5Unit8Servos2Chain servos;
  servos.set_uart_parent(&uart);
  servos.set_chain_index(5);
  servos.set_update_interval(0);
  servos.setup();

  EXPECT_CALL(uart, write_array(::testing::_, ::testing::_)).Times(1);
  servos.loop();
  uart.inject(packet(5, GET_MODE_ALL_COMMAND, {3, 3}));
  servos.loop();

  EXPECT_TRUE(servos.status_has_warning());
}

TEST(M5Unit8Servos2ChainTest, SendsServoAngleRequest) {
  MockUART uart;
  M5Unit8Servos2Chain servos;
  servos.set_uart_parent(&uart);
  servos.set_chain_index(5);

  EXPECT_CALL(uart, write_array(::testing::_, 11)).Times(1);
  EXPECT_EQ(servos.set_servo_angle(0, 90), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  EXPECT_TRUE(servos.request_pending());

  uart.inject(packet(5, SET_SERVO_ANGLE_COMMAND, {1}));
  servos.loop();
  EXPECT_FALSE(servos.status_has_warning());
}

TEST(M5Unit8Servos2ChainTest, RejectsServoAngleOutsideRange) {
  MockUART uart;
  M5Unit8Servos2Chain servos;
  servos.set_uart_parent(&uart);
  servos.set_chain_index(5);

  EXPECT_CALL(uart, write_array(::testing::_, ::testing::_)).Times(0);
  EXPECT_EQ(servos.set_servo_angle(0, 181), m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR);
  EXPECT_EQ(servos.set_servo_angle(8, 90), m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR);
  EXPECT_EQ(servos.set_pwm_duty(0, 101), m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR);
  EXPECT_EQ(servos.set_rgb(16, 0, 0, 0), m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR);
}

TEST(M5Unit8Servos2ChainTest, SendsAllServoAnglesAsOneRequest) {
  MockUART uart;
  M5Unit8Servos2Chain servos;
  std::vector<uint8_t> written;
  servos.set_uart_parent(&uart);
  servos.set_chain_index(5);

  EXPECT_CALL(uart, write_array(::testing::_, 17)).WillOnce([&](const uint8_t *data, size_t size) {
    written.assign(data, data + size);
  });
  EXPECT_EQ(servos.set_servo_angles({0, 45, 90, 135, 180, 90, 45, 0}), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  ASSERT_EQ(written.size(), 17);
  EXPECT_EQ(written[5], SET_SERVO_ANGLE_ALL_COMMAND);
  EXPECT_EQ(written[DATA_OFFSET], 0);
  EXPECT_EQ(written[DATA_OFFSET + 4], 180);
}

TEST(M5Unit8Servos2ChainTest, RejectsAngleListOfWrongLength) {
  MockUART uart;
  M5Unit8Servos2Chain servos;
  servos.set_uart_parent(&uart);
  servos.set_chain_index(5);

  EXPECT_CALL(uart, write_array(::testing::_, ::testing::_)).Times(0);
  EXPECT_EQ(servos.set_servo_angles({0, 45, 90}), m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR);
  EXPECT_EQ(servos.set_pwm_duties({0, 10}), m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR);
  EXPECT_EQ(servos.set_output_levels({true, false}), m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR);
}

TEST(M5Unit8Servos2ChainTest, SendsAllOutputLevelsAsBytes) {
  MockUART uart;
  M5Unit8Servos2Chain servos;
  std::vector<uint8_t> written;
  servos.set_uart_parent(&uart);
  servos.set_chain_index(5);

  EXPECT_CALL(uart, write_array(::testing::_, 17)).WillOnce([&](const uint8_t *data, size_t size) {
    written.assign(data, data + size);
  });
  EXPECT_EQ(servos.set_output_levels({true, false, true, false, true, false, true, false}),
            m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  ASSERT_EQ(written.size(), 17);
  EXPECT_EQ(written[5], SET_OUTPUT_LEVEL_ALL_COMMAND);
  EXPECT_EQ(written[DATA_OFFSET], 1);
  EXPECT_EQ(written[DATA_OFFSET + 1], 0);
  EXPECT_EQ(written[DATA_OFFSET + 7], 0);
}

TEST(M5Unit8Servos2ChainTest, PacksRefreshBitIntoRgbConfig) {
  MockUART uart;
  M5Unit8Servos2Chain servos;
  std::vector<uint8_t> written;
  servos.set_uart_parent(&uart);
  servos.set_chain_index(5);

  EXPECT_CALL(uart, write_array(::testing::_, 11)).WillOnce([&](const uint8_t *data, size_t size) {
    written.assign(data, data + size);
  });
  EXPECT_EQ(servos.set_rgb_config(4, 4, true), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  ASSERT_EQ(written.size(), 11);
  EXPECT_EQ(written[5], SET_RGB_CONFIG_COMMAND);
  EXPECT_EQ(written[DATA_OFFSET], 4);
  EXPECT_EQ(written[DATA_OFFSET + 1], 4 | SERVOS2_RGB_CONFIG_REFRESH);
}

TEST(M5Unit8Servos2ChainTest, RejectsRgbConfigOutsideSupportedRange) {
  MockUART uart;
  M5Unit8Servos2Chain servos;
  servos.set_uart_parent(&uart);
  servos.set_chain_index(5);

  EXPECT_CALL(uart, write_array(::testing::_, ::testing::_)).Times(0);
  EXPECT_EQ(servos.set_rgb_config(0, 0, true), m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR);
  EXPECT_EQ(servos.set_rgb_config(0, 17, true), m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR);
  EXPECT_EQ(servos.set_rgb_config(8, 1, true), m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR);
}

TEST(M5Unit8Servos2ChainTest, OutputAppliesModeBeforeServoValue) {
  MockUART uart;
  M5Unit8Servos2Chain servos;
  servos.set_uart_parent(&uart);
  servos.set_chain_index(5);
  servos.set_update_interval(SCHEDULER_DONT_RUN);
  servos.setup();

  M5Unit8Servos2FloatOutput output;
  output.set_parent(&servos);
  output.set_channel(0);
  output.set_output_type(Servos2OutputType::SERVOS2_OUTPUT_TYPE_SERVO);
  output.setup();

  std::vector<std::vector<uint8_t>> frames;
  EXPECT_CALL(uart, write_array(::testing::_, ::testing::_)).WillRepeatedly([&](const uint8_t *data, size_t size) {
    frames.emplace_back(data, data + size);
  });

  output.set_level(0.5f);

  servos.loop();
  ASSERT_EQ(frames.size(), 1);
  EXPECT_EQ(frames[0][5], SET_MODE_COMMAND);
  EXPECT_EQ(frames[0][DATA_OFFSET], 0);
  EXPECT_EQ(frames[0][DATA_OFFSET + 1], static_cast<uint8_t>(Servos2Mode::SERVOS2_MODE_SERVO));

  deliver(servos, uart, SET_MODE_COMMAND);
  ASSERT_EQ(frames.size(), 2);
  EXPECT_EQ(frames[1][5], SET_SERVO_ANGLE_ALL_COMMAND);
  EXPECT_EQ(frames[1][DATA_OFFSET], 90);

  deliver(servos, uart, SET_SERVO_ANGLE_ALL_COMMAND);
  EXPECT_FALSE(servos.status_has_warning());
}

TEST(M5Unit8Servos2ChainTest, OutputMapsPwmLevelToDuty) {
  MockUART uart;
  M5Unit8Servos2Chain servos;
  servos.set_uart_parent(&uart);
  servos.set_chain_index(5);
  servos.set_update_interval(SCHEDULER_DONT_RUN);
  servos.setup();

  M5Unit8Servos2FloatOutput output;
  output.set_parent(&servos);
  output.set_channel(1);
  output.set_output_type(Servos2OutputType::SERVOS2_OUTPUT_TYPE_PWM);
  output.setup();

  std::vector<std::vector<uint8_t>> frames;
  EXPECT_CALL(uart, write_array(::testing::_, ::testing::_)).WillRepeatedly([&](const uint8_t *data, size_t size) {
    frames.emplace_back(data, data + size);
  });

  output.set_level(0.25f);
  servos.loop();
  deliver(servos, uart, SET_MODE_COMMAND);
  ASSERT_EQ(frames.size(), 2);
  EXPECT_EQ(frames[1][5], 0x81);  // SET_PWM_DUTY_ALL
  EXPECT_EQ(frames[1][DATA_OFFSET + 1], 25);
}

TEST(M5Unit8Servos2ChainTest, NumberPublishesAngleAndRequestsServoMode) {
  MockUART uart;
  M5Unit8Servos2Chain servos;
  servos.set_uart_parent(&uart);
  servos.set_chain_index(5);
  servos.set_update_interval(SCHEDULER_DONT_RUN);
  servos.setup();

  M5Unit8Servos2AngleNumber number;
  number.set_parent(&servos);
  number.set_channel(2);
  number.setup();

  std::vector<std::vector<uint8_t>> frames;
  EXPECT_CALL(uart, write_array(::testing::_, ::testing::_)).WillRepeatedly([&](const uint8_t *data, size_t size) {
    frames.emplace_back(data, data + size);
  });

  number.make_call().set_value(135.0f).perform();
  EXPECT_FLOAT_EQ(number.state, 135.0f);

  servos.loop();
  ASSERT_EQ(frames.size(), 1);
  EXPECT_EQ(frames[0][5], SET_MODE_COMMAND);
  deliver(servos, uart, SET_MODE_COMMAND);
  ASSERT_EQ(frames.size(), 2);
  EXPECT_EQ(frames[1][DATA_OFFSET + 2], 135);
}

TEST(M5Unit8Servos2ChainTest, SwitchPublishesStateAfterAck) {
  MockUART uart;
  M5Unit8Servos2Chain servos;
  servos.set_uart_parent(&uart);
  servos.set_chain_index(5);
  servos.set_update_interval(SCHEDULER_DONT_RUN);
  servos.setup();

  M5Unit8Servos2Switch sw;
  sw.set_parent(&servos);
  sw.set_channel(3);
  sw.set_restore_mode(switch_::SWITCH_ALWAYS_OFF);
  sw.setup();
  sw.turn_on();

  std::vector<std::vector<uint8_t>> frames;
  EXPECT_CALL(uart, write_array(::testing::_, ::testing::_)).WillRepeatedly([&](const uint8_t *data, size_t size) {
    frames.emplace_back(data, data + size);
  });

  servos.loop();
  ASSERT_EQ(frames.size(), 1);
  EXPECT_EQ(frames[0][5], SET_MODE_COMMAND);
  deliver(servos, uart, SET_MODE_COMMAND);
  ASSERT_EQ(frames.size(), 2);
  EXPECT_EQ(frames[1][5], SET_OUTPUT_LEVEL_ALL_COMMAND);
  EXPECT_EQ(frames[1][DATA_OFFSET + 3], 1);
  EXPECT_FALSE(sw.state);

  deliver(servos, uart, SET_OUTPUT_LEVEL_ALL_COMMAND);
  EXPECT_TRUE(sw.state);
}

TEST(M5Unit8Servos2ChainTest, LightWritesOffsetBufferAndLatches) {
  MockUART uart;
  M5Unit8Servos2Chain servos;
  servos.set_uart_parent(&uart);
  servos.set_chain_index(5);
  servos.set_update_interval(SCHEDULER_DONT_RUN);
  servos.setup();

  M5Unit8Servos2Light light;
  light.set_parent(&servos);
  light.set_channel(4);
  light.set_num_leds(2);
  light.set_offset(6);
  light.setup();
  light[0] = Color(255, 0, 0);
  light[1] = Color(0, 255, 0);

  std::vector<std::vector<uint8_t>> frames;
  EXPECT_CALL(uart, write_array(::testing::_, ::testing::_)).WillRepeatedly([&](const uint8_t *data, size_t size) {
    frames.emplace_back(data, data + size);
  });

  light.write_state(nullptr);

  servos.loop();
  ASSERT_EQ(frames.size(), 1);
  EXPECT_EQ(frames[0][5], SET_MODE_COMMAND);
  EXPECT_EQ(frames[0][DATA_OFFSET + 1], static_cast<uint8_t>(Servos2Mode::SERVOS2_MODE_RGB));

  deliver(servos, uart, SET_MODE_COMMAND);
  ASSERT_EQ(frames.size(), 2);
  EXPECT_EQ(frames[1][5], SET_RGB_BUFFER_COMMAND);
  EXPECT_EQ(frames[1][DATA_OFFSET], 6);
  EXPECT_EQ(frames[1][DATA_OFFSET + 1], 255);

  deliver(servos, uart, SET_RGB_BUFFER_COMMAND);
  ASSERT_EQ(frames.size(), 3);
  EXPECT_EQ(frames[2][5], SET_RGB_BUFFER_COMMAND);
  EXPECT_EQ(frames[2][DATA_OFFSET], 7);
  EXPECT_EQ(frames[2][DATA_OFFSET + 2], 255);

  deliver(servos, uart, SET_RGB_BUFFER_COMMAND);
  ASSERT_EQ(frames.size(), 4);
  EXPECT_EQ(frames[3][5], SET_RGB_CONFIG_COMMAND);
  EXPECT_EQ(frames[3][DATA_OFFSET], 4);
  EXPECT_EQ(frames[3][DATA_OFFSET + 1], 2 | SERVOS2_RGB_CONFIG_REFRESH);

  deliver(servos, uart, SET_RGB_CONFIG_COMMAND);
  EXPECT_FALSE(servos.status_has_warning());
}
}  // namespace
}  // namespace esphome::m5_unit_8servos2_chain::testing
