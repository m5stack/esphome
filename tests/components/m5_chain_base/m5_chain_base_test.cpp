#include <algorithm>
#include <cstdint>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "esphome/components/m5_chain_base/m5_chain_base.h"
#include "esphome/components/m5_chain_base/m5_chain_output.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::m5_chain_base::testing {
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

class TestBase : public M5ChainBase {
 public:
  struct FrameRecord {
    uint8_t index;
    uint8_t command;
    std::vector<uint8_t> data;
  };

  std::vector<FrameRecord> responses;
  std::vector<FrameRecord> unsolicited;
  std::vector<std::pair<uint8_t, uint8_t>> timeouts;
  uint8_t last_rgb_component{0};
  uint8_t last_rgb_value{0};
  uint8_t last_brightness{0};

  ChainStatus set_rgb_color_component(uint8_t component, uint8_t value) override {
    this->last_rgb_component = component;
    this->last_rgb_value = value;
    return ChainStatus::CHAIN_STATUS_OK;
  }

  ChainStatus set_led_brightness(uint8_t brightness) override {
    this->last_brightness = brightness;
    return ChainStatus::CHAIN_STATUS_OK;
  }

 protected:
  static FrameRecord record_(const ChainFrame &frame) {
    return {frame.index, frame.command, {frame.data, frame.data + frame.data_size}};
  }
  void on_response(const ChainFrame &frame) override { this->responses.push_back(record_(frame)); }
  void on_unsolicited(const ChainFrame &frame) override { this->unsolicited.push_back(record_(frame)); }
  void on_request_timeout(uint8_t index, uint8_t command) override { this->timeouts.emplace_back(index, command); }
};

static std::vector<uint8_t> packet(uint8_t index, uint8_t command, std::initializer_list<uint8_t> data = {}) {
  std::array<uint8_t, MAX_PACKET_SIZE> buffer{};
  uint16_t size = 0;
  EXPECT_TRUE(M5ChainBase::build_packet(index, command, data.begin(), static_cast<uint16_t>(data.size()), buffer.data(),
                                        buffer.size(), &size));
  return {buffer.begin(), buffer.begin() + size};
}

TEST(M5ChainBaseTest, BuildsAndValidatesEmptyAndDataPackets) {
  const auto empty = packet(1, 0x50);
  EXPECT_EQ(empty, (std::vector<uint8_t>{0xAA, 0x55, 0x03, 0x00, 0x01, 0x50, 0x51, 0x55, 0xAA}));
  EXPECT_TRUE(M5ChainBase::validate_packet(empty.data(), empty.size()));

  const auto data = packet(2, 0x20, {0x00, 0x01, 0xFF});
  EXPECT_TRUE(M5ChainBase::validate_packet(data.data(), data.size()));
  EXPECT_EQ(M5ChainBase::calculate_crc(data.data(), data.size()), data[data.size() - 3]);
}

TEST(M5ChainBaseTest, RejectsMalformedPackets) {
  auto bad = packet(1, 0x50);
  bad[6] ^= 0x01;
  EXPECT_FALSE(M5ChainBase::validate_packet(bad.data(), bad.size()));
  bad = packet(1, 0x50);
  bad[2] = 0xFF;
  EXPECT_FALSE(M5ChainBase::validate_packet(bad.data(), bad.size()));
}

TEST(M5ChainBaseTest, ParsesPartialAndMultiplePackets) {
  MockUART uart;
  TestBase base;
  base.set_uart_parent(&uart);
  const auto first = packet(1, 0x10, {0x01});
  const auto second = packet(2, 0x11, {0x02, 0x03});

  uart.inject({first.begin(), first.begin() + 4});
  base.loop();
  EXPECT_TRUE(base.responses.empty());
  uart.inject({first.begin() + 4, first.end()});
  uart.inject(second);
  base.loop();

  ASSERT_EQ(base.unsolicited.size(), 2u);
  EXPECT_EQ(base.unsolicited[0].index, 1);
  EXPECT_EQ(base.unsolicited[0].data.size(), 1);
  EXPECT_EQ(base.unsolicited[1].index, 2);
  EXPECT_EQ(base.unsolicited[1].data.size(), 2);
}

TEST(M5ChainBaseTest, RecoversFromNoiseAndBadFrame) {
  MockUART uart;
  TestBase base;
  base.set_uart_parent(&uart);
  auto bad = packet(1, 0x10, {0x01});
  bad.back() ^= 0x01;
  const auto good = packet(1, 0x10, {0x02});
  uart.inject({0x00, 0x12, 0xAA});
  uart.inject(bad);
  uart.inject(good);
  base.loop();
  ASSERT_EQ(base.unsolicited.size(), 1u);
  EXPECT_EQ(base.unsolicited[0].data[0], 0x02);
}

TEST(M5ChainBaseTest, MatchesOnePendingRequestAndKeepsOtherFramesUnsolicited) {
  MockUART uart;
  TestBase base;
  base.set_uart_parent(&uart);
  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(1);
  EXPECT_EQ(base.request(3, 0x50), ChainStatus::CHAIN_STATUS_OK);
  EXPECT_TRUE(base.request_pending());

  uart.inject(packet(2, 0x50, {0x99}));
  uart.inject(packet(3, 0x50, {0x01, 0x02}));
  base.loop();

  EXPECT_FALSE(base.request_pending());
  ASSERT_EQ(base.responses.size(), 1u);
  EXPECT_EQ(base.responses[0].index, 3);
  ASSERT_EQ(base.unsolicited.size(), 1u);
  EXPECT_EQ(base.unsolicited[0].index, 2);
}

TEST(M5ChainBaseTest, DispatchesFramesToAllInstancesSharingUart) {
  MockUART uart;
  TestBase first;
  TestBase second;
  first.set_uart_parent(&uart);
  second.set_uart_parent(&uart);

  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(2);
  EXPECT_EQ(first.request(1, 0x50), ChainStatus::CHAIN_STATUS_OK);
  EXPECT_EQ(second.request(2, 0x50), ChainStatus::CHAIN_STATUS_OK);

  uart.inject(packet(1, 0x50, {0x11}));
  uart.inject(packet(2, 0x50, {0x22}));
  first.loop();
  second.loop();

  ASSERT_EQ(first.responses.size(), 1u);
  ASSERT_EQ(second.responses.size(), 1u);
  EXPECT_EQ(first.responses[0].index, 1);
  EXPECT_EQ(first.responses[0].data[0], 0x11);
  EXPECT_EQ(second.responses[0].index, 2);
  EXPECT_EQ(second.responses[0].data[0], 0x22);
}

TEST(M5ChainBaseTest, SupportsDisablingPolling) {
  TestBase base;

  EXPECT_TRUE(base.is_update_interval_enabled());
  base.set_update_interval(SCHEDULER_DONT_RUN);
  EXPECT_FALSE(base.is_update_interval_enabled());
}

TEST(M5ChainBaseTest, SharedRgbOutputUsesBaseVirtualApi) {
  TestBase base;
  M5ChainLedOutput output;
  output.set_parent(&base);

  output.set_channel(LedChannel::RGB_GREEN);
  output.write_state(0.5f);
  EXPECT_EQ(base.last_rgb_component, 1);
  EXPECT_EQ(base.last_rgb_value, 127);

  output.set_channel(LedChannel::RGB_BRIGHTNESS);
  output.write_state(0.75f);
  EXPECT_EQ(base.last_brightness, 75);
}

}  // namespace
}  // namespace esphome::m5_chain_base::testing
