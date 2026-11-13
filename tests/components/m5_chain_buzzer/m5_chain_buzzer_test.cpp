#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "esphome/components/m5_chain_buzzer/m5_chain_buzzer.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::m5_chain_buzzer::testing {
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

static void wait_for_next_bus_slot() {
  std::this_thread::sleep_for(std::chrono::milliseconds(m5_chain_base::INTER_REQUEST_DELAY_MS + 2));
}

TEST(M5ChainBuzzerTest, SendsPlayAutoAndAcceptsAck) {
  MockUART uart;
  M5ChainBuzzer buzzer;
  buzzer.set_uart_parent(&uart);
  buzzer.set_chain_index(7);

  EXPECT_CALL(uart, write_array(::testing::_, 14)).Times(1);
  EXPECT_EQ(buzzer.play_auto(2700, 50, 200), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  EXPECT_TRUE(buzzer.request_pending());

  uart.inject(packet(7, static_cast<uint8_t>(BuzzerCommand::BUZZER_COMMAND_PLAY_AUTO), {1}));
  buzzer.loop();

  EXPECT_FALSE(buzzer.request_pending());
  EXPECT_FALSE(buzzer.status_has_warning());
}

TEST(M5ChainBuzzerTest, RejectsOutOfRangePlayAuto) {
  M5ChainBuzzer buzzer;

  EXPECT_EQ(buzzer.play_auto(99, 50, 200), m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR);
  EXPECT_EQ(buzzer.play_auto(10001, 50, 200), m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR);
  EXPECT_EQ(buzzer.play_auto(2700, 101, 200), m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR);
}

TEST(M5ChainBuzzerTest, QueuesRgbUpdateUntilPendingToneCompletes) {
  MockUART uart;
  M5ChainBuzzer buzzer;
  buzzer.set_uart_parent(&uart);
  buzzer.set_chain_index(7);

  // The tone request goes out immediately, the RGB update is queued behind it.
  EXPECT_CALL(uart, write_array(::testing::_, 14)).Times(2);
  EXPECT_EQ(buzzer.play_auto(2700, 50, 0), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  EXPECT_EQ(buzzer.set_rgb_color(1, 2, 3), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  EXPECT_TRUE(buzzer.request_pending());

  uart.inject(packet(7, static_cast<uint8_t>(BuzzerCommand::BUZZER_COMMAND_PLAY_AUTO), {1}));
  buzzer.loop();
  EXPECT_TRUE(buzzer.request_pending());

  wait_for_next_bus_slot();
  buzzer.loop();
  EXPECT_TRUE(buzzer.request_pending());
  EXPECT_FALSE(buzzer.status_has_warning());
}

TEST(M5ChainBuzzerTest, CombinesRgbComponentsAcrossOutputUpdates) {
  MockUART uart;
  M5ChainBuzzer buzzer;
  buzzer.set_uart_parent(&uart);
  buzzer.set_chain_index(7);

  EXPECT_CALL(uart, write_array(::testing::_, 14)).Times(2);
  EXPECT_EQ(buzzer.set_rgb_color_component(0, 1), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  EXPECT_EQ(buzzer.set_rgb_color_component(1, 2), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  EXPECT_EQ(buzzer.set_rgb_color_component(2, 3), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);

  uart.inject(packet(7, static_cast<uint8_t>(m5_chain_base::ChainCommand::CHAIN_COMMAND_SET_RGB_VALUE), {1}));
  buzzer.loop();
  EXPECT_TRUE(buzzer.request_pending());
  wait_for_next_bus_slot();
  buzzer.loop();
}

TEST(M5ChainBuzzerTest, SendsStatusAndModeCommands) {
  MockUART uart;
  M5ChainBuzzer buzzer;
  buzzer.set_uart_parent(&uart);
  buzzer.set_chain_index(7);

  EXPECT_CALL(uart, write_array(::testing::_, 10)).Times(1);
  EXPECT_EQ(buzzer.set_status(false), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  uart.inject(packet(7, static_cast<uint8_t>(BuzzerCommand::BUZZER_COMMAND_SET_STATUS), {1}));
  buzzer.loop();
  EXPECT_FALSE(buzzer.request_pending());
  EXPECT_FALSE(buzzer.status_has_warning());

  wait_for_next_bus_slot();
  EXPECT_CALL(uart, write_array(::testing::_, 10)).Times(1);
  EXPECT_EQ(buzzer.set_mode(BuzzerMode::BUZZER_MODE_NOTE), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  uart.inject(packet(7, static_cast<uint8_t>(BuzzerCommand::BUZZER_COMMAND_SET_MODE), {1}));
  buzzer.loop();
  EXPECT_FALSE(buzzer.request_pending());
  EXPECT_FALSE(buzzer.status_has_warning());
}

TEST(M5ChainBuzzerTest, HoldsToneUntilStopped) {
  MockUART uart;
  M5ChainBuzzer buzzer;
  buzzer.set_uart_parent(&uart);
  buzzer.set_chain_index(7);

  std::vector<uint8_t> tone;
  EXPECT_CALL(uart, write_array(::testing::_, 14)).WillOnce([&tone](const uint8_t *data, size_t size) {
    tone.assign(data, data + size);
  });

  buzzer.set_tone(880, 50);

  ASSERT_EQ(tone.size(), 14u);
  EXPECT_EQ(tone[4], 7);
  EXPECT_EQ(tone[5], static_cast<uint8_t>(BuzzerCommand::BUZZER_COMMAND_PLAY_AUTO));
  EXPECT_EQ(tone[6] | (tone[7] << 8), 880);
  EXPECT_EQ(tone[8], 50);
  // A held tone uses the longest auto-play duration, the next note replaces it.
  EXPECT_EQ(tone[9] | (tone[10] << 8), 0xFFFF);

  uart.inject(packet(7, static_cast<uint8_t>(BuzzerCommand::BUZZER_COMMAND_PLAY_AUTO), {1}));
  buzzer.loop();
  EXPECT_FALSE(buzzer.request_pending());

  wait_for_next_bus_slot();
  std::vector<uint8_t> silence;
  EXPECT_CALL(uart, write_array(::testing::_, 14)).WillOnce([&silence](const uint8_t *data, size_t size) {
    silence.assign(data, data + size);
  });
  buzzer.set_tone_off();

  ASSERT_EQ(silence.size(), 14u);
  EXPECT_EQ(silence[5], static_cast<uint8_t>(BuzzerCommand::BUZZER_COMMAND_PLAY_AUTO));
  EXPECT_EQ(silence[8], 0);
  EXPECT_EQ(silence[9] | (silence[10] << 8), 1);
}

TEST(M5ChainBuzzerTest, SendsLatestToneOncePendingRequestCompletes) {
  MockUART uart;
  M5ChainBuzzer buzzer;
  buzzer.set_uart_parent(&uart);
  buzzer.set_chain_index(7);

  std::vector<std::vector<uint8_t>> frames;
  EXPECT_CALL(uart, write_array(::testing::_, 14)).Times(2).WillRepeatedly([&frames](const uint8_t *data, size_t size) {
    frames.emplace_back(data, data + size);
  });

  buzzer.set_tone(2700, 50);
  buzzer.set_tone(440, 50);
  ASSERT_EQ(frames.size(), 1u);

  uart.inject(packet(7, static_cast<uint8_t>(BuzzerCommand::BUZZER_COMMAND_PLAY_AUTO), {1}));
  wait_for_next_bus_slot();
  buzzer.loop();

  ASSERT_EQ(frames.size(), 2u);
  EXPECT_EQ(frames[0][6] | (frames[0][7] << 8), 2700);
  EXPECT_EQ(frames[1][6] | (frames[1][7] << 8), 440);
}

TEST(M5ChainBuzzerTest, DoesNotSendSilenceFrameWhenIdle) {
  MockUART uart;
  M5ChainBuzzer buzzer;
  buzzer.set_uart_parent(&uart);
  buzzer.set_chain_index(7);

  EXPECT_CALL(uart, write_array(::testing::_, ::testing::_)).Times(0);
  buzzer.set_tone_off();

  wait_for_next_bus_slot();
  buzzer.loop();
  EXPECT_FALSE(buzzer.request_pending());
}

}  // namespace
}  // namespace esphome::m5_chain_buzzer::testing
