#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "esphome/components/m5_chain_mono/m5_chain_mono.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::m5_chain_mono::testing {
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

/// A 17 byte frame occupies the wire for about 2 ms and is followed by the pipelined pacing gap.
static void wait_for_the_frame_to_clear() { std::this_thread::sleep_for(std::chrono::milliseconds(20)); }

TEST(M5ChainMonoTest, SendsFullFrameForDisplayBuffer) {
  MockUART uart;
  M5ChainMono mono;
  mono.set_uart_parent(&uart);
  mono.set_chain_index(1);

  const std::array<uint8_t, 8> buffer{0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81};
  std::vector<uint8_t> frame;
  EXPECT_CALL(uart, write_array(::testing::_, 17)).WillOnce([&frame](const uint8_t *data, size_t size) {
    frame.assign(data, data + size);
  });

  EXPECT_EQ(mono.set_display_buffer(buffer.data(), buffer.size()), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);

  // Length 0x000B covers index, command, the eight buffer bytes and the CRC; the CRC is a plain sum.
  uint16_t checksum = 1 + static_cast<uint8_t>(MonoCommand::MONO_COMMAND_SET_DISPLAY_BUFFER);
  for (uint8_t byte : buffer)
    checksum += byte;
  std::vector<uint8_t> expected{0xAA, 0x55, 0x0B, 0x00, 0x01};
  expected.push_back(static_cast<uint8_t>(MonoCommand::MONO_COMMAND_SET_DISPLAY_BUFFER));
  expected.insert(expected.end(), buffer.begin(), buffer.end());
  expected.push_back(static_cast<uint8_t>(checksum & 0xFF));
  expected.push_back(0x55);
  expected.push_back(0xAA);
  EXPECT_EQ(frame, expected);
}

TEST(M5ChainMonoTest, RejectsBufferOfWrongSize) {
  MockUART uart;
  M5ChainMono mono;
  mono.set_uart_parent(&uart);

  const std::array<uint8_t, 8> buffer{};
  EXPECT_EQ(mono.set_display_buffer(buffer.data(), 7), m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR);
  EXPECT_EQ(mono.set_display_buffer(nullptr, buffer.size()), m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR);
  EXPECT_FALSE(mono.request_pending());
}

TEST(M5ChainMonoTest, ReplacesTheHeldFrameWhileTheBusIsBusy) {
  MockUART uart;
  M5ChainMono mono;
  mono.set_uart_parent(&uart);
  mono.set_chain_index(1);

  std::array<uint8_t, 8> first{};
  first[0] = 0x80;
  std::array<uint8_t, 8> second{};
  second[0] = 0x81;

  std::vector<std::vector<uint8_t>> frames;
  EXPECT_CALL(uart, write_array(::testing::_, ::testing::_))
      .WillRepeatedly([&frames](const uint8_t *data, size_t size) { frames.emplace_back(data, data + size); });
  EXPECT_EQ(mono.set_display_buffer(first.data(), first.size()), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  EXPECT_EQ(mono.set_display_buffer(second.data(), second.size()), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  ASSERT_EQ(frames.size(), 1u) << "the second update only replaces the queued frame";
  EXPECT_EQ(frames[0][6], 0x80);
  EXPECT_TRUE(mono.request_pending()) << "the held frame waits for the first one to clear the wire";

  wait_for_the_frame_to_clear();
  mono.loop();
  ASSERT_EQ(frames.size(), 2u);
  EXPECT_EQ(frames[1][6], 0x81);
  // A frame is not acknowledged: the bus is free as soon as it has been written.
  EXPECT_FALSE(mono.request_pending());
  EXPECT_FALSE(mono.status_has_warning());
}

TEST(M5ChainMonoTest, SetsWarningWhenTheScreenBrightnessIsRejected) {
  MockUART uart;
  M5ChainMono mono;
  mono.set_uart_parent(&uart);
  mono.set_chain_index(1);

  EXPECT_CALL(uart, write_array(::testing::_, 11)).Times(1);
  EXPECT_EQ(mono.set_screen_brightness(MAX_SCREEN_BRIGHTNESS_LEVEL), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);

  uart.inject(packet(1, static_cast<uint8_t>(MonoCommand::MONO_COMMAND_SET_BRIGHTNESS), {0}));
  mono.loop();

  EXPECT_FALSE(mono.request_pending());
  EXPECT_TRUE(mono.status_has_warning());
}

TEST(M5ChainMonoTest, SendsTheScreenBrightnessWithoutSavingIt) {
  MockUART uart;
  M5ChainMono mono;
  mono.set_uart_parent(&uart);
  mono.set_chain_index(2);

  std::vector<std::vector<uint8_t>> frames;
  EXPECT_CALL(uart, write_array(::testing::_, ::testing::_))
      .WillRepeatedly([&frames](const uint8_t *data, size_t size) { frames.emplace_back(data, data + size); });

  wait_for_next_bus_slot();
  EXPECT_EQ(mono.set_screen_brightness(4), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  mono.loop();
  // The same level is not put on the wire a second time.
  EXPECT_EQ(mono.set_screen_brightness(4), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  mono.loop();

  ASSERT_EQ(frames.size(), 1u);
  EXPECT_EQ(frames[0][4], 2);
  EXPECT_EQ(frames[0][5], static_cast<uint8_t>(MonoCommand::MONO_COMMAND_SET_BRIGHTNESS));
  EXPECT_EQ(frames[0][6], 4) << "Brightness_level";
  EXPECT_EQ(frames[0][7], 0) << "Save_to_flash stays clear";
}

TEST(M5ChainMonoTest, RejectsScreenBrightnessAboveSeven) {
  MockUART uart;
  M5ChainMono mono;
  mono.set_uart_parent(&uart);
  mono.set_chain_index(1);

  EXPECT_CALL(uart, write_array(::testing::_, ::testing::_)).Times(0);
  EXPECT_EQ(mono.set_screen_brightness(MAX_SCREEN_BRIGHTNESS_LEVEL + 1),
            m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR);
  EXPECT_FALSE(mono.request_pending());
}

}  // namespace
}  // namespace esphome::m5_chain_mono::testing
