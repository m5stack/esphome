#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/m5_chain_mic/m5_chain_mic.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::m5_chain_mic::testing {
namespace {

static constexpr uint8_t MIC_GET_ADC_12_COMMAND = 0x30;
static constexpr uint8_t MIC_GET_ADC_8_COMMAND = 0x31;
static constexpr uint8_t MIC_GET_THRESHOLD_COMMAND = 0x33;
static constexpr uint8_t MIC_TRIGGER_REPORT_COMMAND = 0xE0;
static constexpr uint8_t MIC_SET_REPORT_MODE_COMMAND = 0xE1;
static constexpr uint8_t MIC_SET_TRIGGER_INTERVAL_COMMAND = 0xE3;
/// Report type code from the protocol document.
static constexpr uint8_t MIC_TRIGGER_REPORT_TYPE = 0x04;
/// Report type code the internal firmware actually sends.
static constexpr uint8_t MIC_TRIGGER_REPORT_TYPE_INTERNAL_FW = 0x03;

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

TEST(M5ChainMicTest, Publishes12BitAdcFromPolledResponse) {
  MockUART uart;
  M5ChainMic mic;
  sensor::Sensor adc;
  mic.set_uart_parent(&uart);
  mic.set_chain_index(2);
  mic.set_update_interval(0);
  mic.set_mic_sensor(MicSensorType::MIC_SENSOR_ADC_12, &adc);

  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(1);
  mic.setup();
  mic.loop();
  // 0x0ABC little endian.
  uart.inject(packet(2, MIC_GET_ADC_12_COMMAND, {0xBC, 0x0A}));
  mic.loop();

  EXPECT_FLOAT_EQ(adc.get_state(), 0x0ABC);
  EXPECT_FALSE(mic.status_has_warning());
}

TEST(M5ChainMicTest, Publishes8BitAdcWhenOnlyThatSensorIsConfigured) {
  MockUART uart;
  M5ChainMic mic;
  sensor::Sensor adc;
  mic.set_uart_parent(&uart);
  mic.set_chain_index(2);
  mic.set_update_interval(0);
  mic.set_mic_sensor(MicSensorType::MIC_SENSOR_ADC_8, &adc);

  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(1);
  mic.setup();
  mic.loop();
  uart.inject(packet(2, MIC_GET_ADC_8_COMMAND, {0x7F}));
  mic.loop();

  EXPECT_FLOAT_EQ(adc.get_state(), 0x7F);
}

TEST(M5ChainMicTest, PublishesThresholdTriggerFromReport) {
  MockUART uart;
  M5ChainMic mic;
  binary_sensor::BinarySensor trigger;
  mic.set_uart_parent(&uart);
  mic.set_chain_index(2);
  mic.set_trigger_sensor(&trigger);
  mic.setup();

  uart.inject(packet(2, MIC_TRIGGER_REPORT_COMMAND, {1, MIC_TRIGGER_REPORT_TYPE}));
  mic.loop();
  EXPECT_TRUE(trigger.get_state());

  uart.inject(packet(2, MIC_TRIGGER_REPORT_COMMAND, {0, MIC_TRIGGER_REPORT_TYPE}));
  mic.loop();
  EXPECT_FALSE(trigger.get_state());
  EXPECT_FALSE(mic.status_has_warning());
}

TEST(M5ChainMicTest, AcceptsThresholdTriggerReportTypeSentByFirmware) {
  MockUART uart;
  M5ChainMic mic;
  binary_sensor::BinarySensor trigger;
  mic.set_uart_parent(&uart);
  mic.set_chain_index(2);
  mic.set_trigger_sensor(&trigger);
  mic.setup();

  uart.inject(packet(2, MIC_TRIGGER_REPORT_COMMAND, {1, MIC_TRIGGER_REPORT_TYPE_INTERNAL_FW}));
  mic.loop();
  EXPECT_TRUE(trigger.get_state());
  EXPECT_FALSE(mic.status_has_warning());

  uart.inject(packet(2, MIC_TRIGGER_REPORT_COMMAND, {0, MIC_TRIGGER_REPORT_TYPE_INTERNAL_FW}));
  mic.loop();
  EXPECT_FALSE(trigger.get_state());
  EXPECT_FALSE(mic.status_has_warning());
}

TEST(M5ChainMicTest, ReportsWarningForReportWithoutDeviceTypeCode) {
  MockUART uart;
  M5ChainMic mic;
  binary_sensor::BinarySensor trigger;
  mic.set_uart_parent(&uart);
  mic.set_chain_index(2);
  mic.set_trigger_sensor(&trigger);
  mic.setup();

  uart.inject(packet(2, MIC_TRIGGER_REPORT_COMMAND, {1, 0x05}));
  mic.loop();

  EXPECT_FALSE(trigger.has_state());
  EXPECT_TRUE(mic.status_has_warning());
}

TEST(M5ChainMicTest, DoesNotPollWithoutSensors) {
  MockUART uart;
  M5ChainMic mic;
  mic.set_uart_parent(&uart);
  mic.set_update_interval(0);

  EXPECT_CALL(uart, write_array(::testing::_, ::testing::_)).Times(0);
  mic.setup();
  mic.loop();
}

TEST(M5ChainMicTest, PublishesThresholdReadFromDevice) {
  MockUART uart;
  M5ChainMic mic;
  sensor::Sensor threshold;
  mic.set_uart_parent(&uart);
  mic.set_chain_index(2);
  mic.set_update_interval(0);
  mic.set_mic_sensor(MicSensorType::MIC_SENSOR_THRESHOLD, &threshold);

  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(1);
  mic.setup();
  mic.loop();
  // 500 little endian, the level the internal firmware uses by default.
  uart.inject(packet(2, MIC_GET_THRESHOLD_COMMAND, {0xF4, 0x01}));
  mic.loop();

  EXPECT_FLOAT_EQ(threshold.get_state(), 500);
  EXPECT_FALSE(mic.status_has_warning());
}

TEST(M5ChainMicTest, SendsReportModeRequest) {
  MockUART uart;
  M5ChainMic mic;
  mic.set_uart_parent(&uart);
  mic.set_chain_index(2);

  EXPECT_CALL(uart, write_array(::testing::_, 10)).Times(1);
  EXPECT_EQ(mic.set_report_mode(true), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);

  uart.inject(packet(2, MIC_SET_REPORT_MODE_COMMAND, {1}));
  mic.loop();
  EXPECT_FALSE(mic.request_pending());
}

TEST(M5ChainMicTest, RejectsOutOfRangeTriggerInterval) {
  MockUART uart;
  M5ChainMic mic;
  mic.set_uart_parent(&uart);
  mic.set_chain_index(2);

  EXPECT_CALL(uart, write_array(::testing::_, ::testing::_)).Times(0);
  EXPECT_EQ(mic.set_trigger_interval(299), m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR);
  EXPECT_EQ(mic.set_trigger_interval(1001), m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR);
}

TEST(M5ChainMicTest, SendsTriggerIntervalRequest) {
  MockUART uart;
  M5ChainMic mic;
  mic.set_uart_parent(&uart);
  mic.set_chain_index(2);

  EXPECT_CALL(uart, write_array(::testing::_, 11)).Times(1);
  EXPECT_EQ(mic.set_trigger_interval(300), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);

  uart.inject(packet(2, MIC_SET_TRIGGER_INTERVAL_COMMAND, {1}));
  mic.loop();
  EXPECT_FALSE(mic.request_pending());
}

}  // namespace
}  // namespace esphome::m5_chain_mic::testing
