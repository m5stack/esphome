#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/m5_chain_switch/m5_chain_switch.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::m5_chain_switch::testing {
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

class TestSwitch : public switch_::Switch {
 protected:
  void write_state(bool state) override {
    if (this->parent_ != nullptr)
      this->parent_->set_event_trigger(state);
  }

 public:
  void set_parent(M5ChainSwitch *parent) { this->parent_ = parent; }

 protected:
  M5ChainSwitch *parent_{nullptr};
};

static std::vector<uint8_t> packet(uint8_t index, uint8_t command, std::initializer_list<uint8_t> data = {}) {
  std::array<uint8_t, m5_chain_base::MAX_PACKET_SIZE> buffer{};
  uint16_t size = 0;
  EXPECT_TRUE(m5_chain_base::M5ChainBase::build_packet(index, command, data.begin(), data.size(), buffer.data(),
                                                       buffer.size(), &size));
  return {buffer.begin(), buffer.begin() + size};
}

TEST(M5ChainSwitchTest, PublishesEventTriggerStateFromProtocol) {
  MockUART uart;
  M5ChainSwitch chain_switch;
  TestSwitch event_trigger;
  chain_switch.set_uart_parent(&uart);
  chain_switch.set_chain_index(7);
  chain_switch.set_event_trigger_switch(&event_trigger);

  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(1);
  chain_switch.setup();
  uart.inject(packet(7, static_cast<uint8_t>(SwitchCommand::SWITCH_COMMAND_GET_EVENT_TRIGGER), {1}));
  chain_switch.loop();

  EXPECT_TRUE(event_trigger.state);
}

TEST(M5ChainSwitchTest, ConfirmsEventTriggerWrite) {
  MockUART uart;
  M5ChainSwitch chain_switch;
  TestSwitch event_trigger;
  chain_switch.set_uart_parent(&uart);
  chain_switch.set_chain_index(4);
  chain_switch.set_event_trigger_switch(&event_trigger);
  event_trigger.set_parent(&chain_switch);

  EXPECT_CALL(uart, write_array(::testing::_, 10)).Times(1);
  event_trigger.turn_on();
  EXPECT_TRUE(chain_switch.request_pending());

  uart.inject(packet(4, static_cast<uint8_t>(SwitchCommand::SWITCH_COMMAND_SET_EVENT_TRIGGER), {1}));
  chain_switch.loop();

  EXPECT_TRUE(event_trigger.state);
  EXPECT_FALSE(chain_switch.request_pending());
}

TEST(M5ChainSwitchTest, PublishesAdcModeThresholdAndStatusSensors) {
  MockUART uart;
  M5ChainSwitch chain_switch;
  sensor::Sensor adc;
  sensor::Sensor change_mode;
  sensor::Sensor open_threshold;
  sensor::Sensor close_threshold;
  binary_sensor::BinarySensor status;
  chain_switch.set_uart_parent(&uart);
  chain_switch.set_chain_index(6);
  chain_switch.set_switch_sensor(SwitchSensorType::SWITCH_SENSOR_ADC_8, &adc);
  chain_switch.set_switch_sensor(SwitchSensorType::SWITCH_SENSOR_CHANGE_MODE, &change_mode);
  chain_switch.set_switch_sensor(SwitchSensorType::SWITCH_SENSOR_OPEN_THRESHOLD, &open_threshold);
  chain_switch.set_switch_sensor(SwitchSensorType::SWITCH_SENSOR_CLOSE_THRESHOLD, &close_threshold);
  chain_switch.set_status_sensor(&status);

  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(5);
  chain_switch.setup();

  uart.inject(packet(6, static_cast<uint8_t>(SwitchCommand::SWITCH_COMMAND_GET_ADC_8), {200}));
  chain_switch.loop();
  EXPECT_FLOAT_EQ(adc.state, 200);

  uart.inject(packet(6, static_cast<uint8_t>(SwitchCommand::SWITCH_COMMAND_GET_CHANGE_MODE), {1}));
  chain_switch.loop();
  EXPECT_FLOAT_EQ(change_mode.state, 1);

  uart.inject(packet(6, static_cast<uint8_t>(SwitchCommand::SWITCH_COMMAND_GET_THRESHOLD), {0x7F, 0x0F, 0x80, 0x00}));
  chain_switch.loop();
  EXPECT_FLOAT_EQ(open_threshold.state, 3967);
  EXPECT_FLOAT_EQ(close_threshold.state, 128);

  uart.inject(packet(6, static_cast<uint8_t>(SwitchCommand::SWITCH_COMMAND_GET_STATUS), {1}));
  chain_switch.loop();
  EXPECT_TRUE(status.get_state());
}

TEST(M5ChainSwitchTest, PublishesUnsolicitedStateChangeReport) {
  MockUART uart;
  M5ChainSwitch chain_switch;
  binary_sensor::BinarySensor status;
  chain_switch.set_uart_parent(&uart);
  chain_switch.set_chain_index(3);
  chain_switch.set_status_sensor(&status);

  uart.inject(packet(3, 0xE0, {1, 0x04}));
  chain_switch.loop();

  EXPECT_TRUE(status.get_state());
  EXPECT_FALSE(chain_switch.status_has_warning());
}

TEST(M5ChainSwitchTest, CombinesRgbComponentsAcrossOutputUpdates) {
  MockUART uart;
  M5ChainSwitch chain_switch;
  chain_switch.set_uart_parent(&uart);
  chain_switch.set_chain_index(4);

  EXPECT_CALL(uart, write_array(::testing::_, 14)).Times(2);
  EXPECT_EQ(chain_switch.set_rgb_color_component(0, 1), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  EXPECT_EQ(chain_switch.set_rgb_color_component(1, 2), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  EXPECT_EQ(chain_switch.set_rgb_color_component(2, 3), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);

  uart.inject(packet(4, static_cast<uint8_t>(m5_chain_base::ChainCommand::CHAIN_COMMAND_SET_RGB_VALUE), {1}));
  chain_switch.loop();
  EXPECT_TRUE(chain_switch.request_pending());
}

}  // namespace
}  // namespace esphome::m5_chain_switch::testing
