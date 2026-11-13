#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "esphome/components/m5_chain_rgb/m5_chain_rgb.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::m5_chain_rgb::testing {
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

/// A 137 byte frame occupies the wire for about 12 ms and is followed by the pipelined pacing gap.
static void wait_for_the_frame_to_clear() { std::this_thread::sleep_for(std::chrono::milliseconds(30)); }

TEST(M5ChainRGBTest, SendsFullFrameForDisplayBuffer) {
  MockUART uart;
  M5ChainRGB rgb;
  rgb.set_uart_parent(&uart);
  rgb.set_chain_index(2);

  std::array<uint8_t, 128> buffer{};
  buffer[0] = 0x00;  // little endian red in RGB565
  buffer[1] = 0xF8;
  std::vector<uint8_t> frame;
  EXPECT_CALL(uart, write_array(::testing::_, 137)).WillOnce([&frame](const uint8_t *data, size_t size) {
    frame.assign(data, data + size);
  });

  EXPECT_EQ(rgb.set_display_buffer(buffer.data(), buffer.size()), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);

  // Length 0x0083 covers index, command, the 128 buffer bytes and the CRC; the CRC is a plain sum.
  uint16_t checksum = 2 + static_cast<uint8_t>(RgbCommand::RGB_COMMAND_SET_DISPLAY_BUFFER);
  for (uint8_t byte : buffer)
    checksum += byte;
  std::vector<uint8_t> expected{0xAA, 0x55, 0x83, 0x00, 0x02};
  expected.push_back(static_cast<uint8_t>(RgbCommand::RGB_COMMAND_SET_DISPLAY_BUFFER));
  expected.insert(expected.end(), buffer.begin(), buffer.end());
  expected.push_back(static_cast<uint8_t>(checksum & 0xFF));
  expected.push_back(0x55);
  expected.push_back(0xAA);
  EXPECT_EQ(frame, expected);
}

TEST(M5ChainRGBTest, RejectsBufferOfWrongSize) {
  MockUART uart;
  M5ChainRGB rgb;
  rgb.set_uart_parent(&uart);

  std::array<uint8_t, 128> buffer{};
  EXPECT_EQ(rgb.set_display_buffer(buffer.data(), 127), m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR);
  EXPECT_EQ(rgb.set_display_buffer(nullptr, buffer.size()), m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR);
  EXPECT_FALSE(rgb.request_pending());
}

TEST(M5ChainRGBTest, ReplacesTheHeldFrameWhileTheBusIsBusy) {
  MockUART uart;
  M5ChainRGB rgb;
  rgb.set_uart_parent(&uart);
  rgb.set_chain_index(2);

  std::array<uint8_t, 128> first{};
  first[1] = 0xF8;
  std::array<uint8_t, 128> second{};
  second[1] = 0x07;

  std::vector<std::vector<uint8_t>> frames;
  EXPECT_CALL(uart, write_array(::testing::_, ::testing::_))
      .WillRepeatedly([&frames](const uint8_t *data, size_t size) { frames.emplace_back(data, data + size); });
  EXPECT_EQ(rgb.set_display_buffer(first.data(), first.size()), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  EXPECT_EQ(rgb.set_display_buffer(second.data(), second.size()), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  ASSERT_EQ(frames.size(), 1u) << "the second update only replaces the queued frame";
  EXPECT_EQ(frames[0][7], 0xF8);
  EXPECT_TRUE(rgb.request_pending()) << "the held frame waits for the first one to clear the wire";

  wait_for_the_frame_to_clear();
  rgb.loop();
  ASSERT_EQ(frames.size(), 2u);
  EXPECT_EQ(frames[1][7], 0x07);
  // A frame is not acknowledged: the bus is free as soon as it has been written.
  EXPECT_FALSE(rgb.request_pending());
  EXPECT_FALSE(rgb.status_has_warning());
}

TEST(M5ChainRGBTest, SendsTheScreenBrightnessWithoutSavingIt) {
  MockUART uart;
  M5ChainRGB rgb;
  rgb.set_uart_parent(&uart);
  rgb.set_chain_index(2);

  std::vector<std::vector<uint8_t>> frames;
  EXPECT_CALL(uart, write_array(::testing::_, ::testing::_))
      .WillRepeatedly([&frames](const uint8_t *data, size_t size) { frames.emplace_back(data, data + size); });

  wait_for_next_bus_slot();
  EXPECT_EQ(rgb.set_screen_brightness(MAX_SCREEN_BRIGHTNESS), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  rgb.loop();
  // The same value is not put on the wire a second time.
  EXPECT_EQ(rgb.set_screen_brightness(MAX_SCREEN_BRIGHTNESS), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  rgb.loop();

  ASSERT_EQ(frames.size(), 1u);
  EXPECT_EQ(frames[0][4], 2);
  EXPECT_EQ(frames[0][5], static_cast<uint8_t>(RgbCommand::RGB_COMMAND_SET_BRIGHTNESS));
  EXPECT_EQ(frames[0][6], MAX_SCREEN_BRIGHTNESS) << "Brightness";
  EXPECT_EQ(frames[0][7], 0) << "Save_to_flash stays clear";
}

TEST(M5ChainRGBTest, RejectsScreenBrightnessAboveOneHundred) {
  MockUART uart;
  M5ChainRGB rgb;
  rgb.set_uart_parent(&uart);
  rgb.set_chain_index(2);

  EXPECT_CALL(uart, write_array(::testing::_, ::testing::_)).Times(0);
  EXPECT_EQ(rgb.set_screen_brightness(MAX_SCREEN_BRIGHTNESS + 1),
            m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR);
  EXPECT_FALSE(rgb.request_pending());
}

TEST(M5ChainRGBTest, SetsWarningWhenTheScreenBrightnessIsRejected) {
  MockUART uart;
  M5ChainRGB rgb;
  rgb.set_uart_parent(&uart);
  rgb.set_chain_index(2);

  EXPECT_CALL(uart, write_array(::testing::_, 11)).Times(1);
  EXPECT_EQ(rgb.set_screen_brightness(MAX_SCREEN_BRIGHTNESS), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);

  uart.inject(packet(2, static_cast<uint8_t>(RgbCommand::RGB_COMMAND_SET_BRIGHTNESS), {0}));
  rgb.loop();

  EXPECT_FALSE(rgb.request_pending());
  EXPECT_TRUE(rgb.status_has_warning());
}

TEST(M5ChainRGBTest, SendsTheHeldFrameOnceTheBrightnessIsAcknowledged) {
  MockUART uart;
  M5ChainRGB rgb;
  rgb.set_uart_parent(&uart);
  rgb.set_chain_index(2);

  std::array<uint8_t, 128> buffer{};
  buffer[1] = 0xF8;

  std::vector<std::vector<uint8_t>> frames;
  EXPECT_CALL(uart, write_array(::testing::_, ::testing::_))
      .WillRepeatedly([&frames](const uint8_t *data, size_t size) { frames.emplace_back(data, data + size); });

  wait_for_next_bus_slot();
  EXPECT_EQ(rgb.set_screen_brightness(MAX_SCREEN_BRIGHTNESS), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  rgb.loop();
  EXPECT_EQ(rgb.set_display_buffer(buffer.data(), buffer.size()), m5_chain_base::ChainStatus::CHAIN_STATUS_OK);
  rgb.loop();
  // The frame waits behind the brightness round trip, so the panel is never shown at the old level.
  ASSERT_EQ(frames.size(), 1u);
  EXPECT_EQ(frames[0][5], static_cast<uint8_t>(RgbCommand::RGB_COMMAND_SET_BRIGHTNESS));

  uart.inject(packet(2, static_cast<uint8_t>(RgbCommand::RGB_COMMAND_SET_BRIGHTNESS), {1}));
  rgb.loop();
  wait_for_the_frame_to_clear();
  rgb.loop();
  ASSERT_EQ(frames.size(), 2u);
  EXPECT_EQ(frames[1][5], static_cast<uint8_t>(RgbCommand::RGB_COMMAND_SET_DISPLAY_BUFFER));
  EXPECT_EQ(frames[1][7], 0xF8);
  EXPECT_FALSE(rgb.status_has_warning());
}

}  // namespace
}  // namespace esphome::m5_chain_rgb::testing
