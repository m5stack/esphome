#include <algorithm>
#include <chrono>
#include <cstdint>
#include <thread>
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
  uint8_t poll_count{0};
  bool polling_enabled{false};
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
  void poll_next_() override {
    if (!this->polling_enabled)
      return;
    this->poll_count++;
    this->request(this->get_chain_index(), 0x50);
  }
};

/// Re-queues the next frame as soon as a response arrives, like the display and light frames of a
/// Chain Mono or Chain RGB do while a panel is being refreshed.
class StreamingBase : public TestBase {
 public:
  void start_stream() {
    this->request(this->get_chain_index(), 0x31, this->frame_.data(), static_cast<uint16_t>(this->frame_.size()));
  }

 protected:
  void on_response(const ChainFrame &frame) override {
    TestBase::on_response(frame);
    this->request(this->get_chain_index(), 0x31, this->frame_.data(), static_cast<uint16_t>(this->frame_.size()));
  }

  std::array<uint8_t, 8> frame_{};
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

TEST(M5ChainBaseTest, ParsesLargeReceiveBurstsWithoutDroppingFrames) {
  MockUART uart;
  TestBase base;
  base.set_uart_parent(&uart);
  std::vector<uint8_t> burst;
  for (uint8_t value = 0; value < 100; value++) {
    const auto frame = packet(1, 0x50, {value});
    burst.insert(burst.end(), frame.begin(), frame.end());
  }

  uart.inject(burst);
  base.loop();

  EXPECT_EQ(base.unsolicited.size(), 100u);
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

TEST(M5ChainBaseTest, DoesNotMatchResponseForRequestUntilItHasBeenSent) {
  MockUART uart;
  TestBase first;
  TestBase second;
  first.set_uart_parent(&uart);
  second.set_uart_parent(&uart);

  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(1);
  EXPECT_EQ(first.request(1, 0x50), ChainStatus::CHAIN_STATUS_OK);
  EXPECT_EQ(second.request(2, 0x50), ChainStatus::CHAIN_STATUS_OK);
  ::testing::Mock::VerifyAndClearExpectations(&uart);

  uart.inject(packet(2, 0x50, {0x22}));
  first.loop();

  EXPECT_TRUE(second.request_pending());
  EXPECT_TRUE(second.responses.empty());
  ASSERT_EQ(second.unsolicited.size(), 1u);
  EXPECT_EQ(second.unsolicited[0].index, 2);

  uart.inject(packet(1, 0x50, {0x11}));
  first.loop();
  std::this_thread::sleep_for(std::chrono::milliseconds(INTER_REQUEST_DELAY_MS + 2));
  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(1);
  first.loop();

  uart.inject(packet(2, 0x50, {0x33}));
  first.loop();
  EXPECT_FALSE(second.request_pending());
  ASSERT_EQ(second.responses.size(), 1u);
  EXPECT_EQ(second.responses[0].data[0], 0x33);
}

TEST(M5ChainBaseTest, SerializesRequestsOnSharedUartAndEnforcesInterRequestDelay) {
  MockUART uart;
  TestBase first;
  TestBase second;
  first.set_uart_parent(&uart);
  second.set_uart_parent(&uart);

  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(1);
  EXPECT_EQ(first.request(1, 0x50), ChainStatus::CHAIN_STATUS_OK);
  EXPECT_EQ(second.request(2, 0x50), ChainStatus::CHAIN_STATUS_OK);
  EXPECT_TRUE(first.request_pending());
  EXPECT_TRUE(second.request_pending());

  uart.inject(packet(1, 0x50, {0x11}));
  first.loop();
  second.loop();

  ASSERT_EQ(first.responses.size(), 1u);
  EXPECT_EQ(first.responses[0].index, 1);
  EXPECT_EQ(first.responses[0].data[0], 0x11);
  EXPECT_TRUE(second.request_pending());
  EXPECT_TRUE(second.responses.empty());
  ::testing::Mock::VerifyAndClearExpectations(&uart);

  std::this_thread::sleep_for(std::chrono::milliseconds(INTER_REQUEST_DELAY_MS + 2));
  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(1);
  first.loop();
  ::testing::Mock::VerifyAndClearExpectations(&uart);

  uart.inject(packet(2, 0x50, {0x22}));
  first.loop();
  ASSERT_EQ(second.responses.size(), 1u);
  EXPECT_EQ(second.responses[0].index, 2);
  EXPECT_EQ(second.responses[0].data[0], 0x22);
}

TEST(M5ChainBaseTest, RetriesTimedOutRequestAfterBackoff) {
  MockUART uart;
  TestBase base;
  base.set_uart_parent(&uart);
  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(1);
  EXPECT_EQ(base.request(1, 0x50, 2), ChainStatus::CHAIN_STATUS_OK);
  ::testing::Mock::VerifyAndClearExpectations(&uart);

  std::this_thread::sleep_for(std::chrono::milliseconds(4));
  base.loop();
  EXPECT_TRUE(base.request_pending());
  EXPECT_TRUE(base.timeouts.empty());

  std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_BACKOFF_MS + INTER_REQUEST_DELAY_MS + 2));
  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(1);
  base.loop();
  ::testing::Mock::VerifyAndClearExpectations(&uart);

  uart.inject(packet(1, 0x50, {0x33}));
  base.loop();
  EXPECT_FALSE(base.request_pending());
  ASSERT_EQ(base.responses.size(), 1u);
  EXPECT_EQ(base.responses[0].data[0], 0x33);
}

TEST(M5ChainBaseTest, PollingUsesRoundRobinAcrossSharedUartInstances) {
  MockUART uart;
  TestBase first;
  TestBase second;
  first.set_uart_parent(&uart);
  first.set_chain_index(1);
  first.set_update_interval(0);
  first.polling_enabled = true;
  second.set_uart_parent(&uart);
  second.set_chain_index(2);
  second.set_update_interval(0);
  second.polling_enabled = true;

  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(1);
  first.loop();
  EXPECT_EQ(first.poll_count, 1);
  EXPECT_EQ(second.poll_count, 0);
  ::testing::Mock::VerifyAndClearExpectations(&uart);

  uart.inject(packet(1, 0x50, {0x11}));
  first.loop();
  std::this_thread::sleep_for(std::chrono::milliseconds(INTER_REQUEST_DELAY_MS + 2));

  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(1);
  first.loop();
  EXPECT_EQ(first.poll_count, 1);
  EXPECT_EQ(second.poll_count, 1);
}

TEST(M5ChainBaseTest, PollingSkipsInstancesWithoutConfiguredRequests) {
  MockUART uart;
  TestBase first;
  TestBase second;
  first.set_uart_parent(&uart);
  first.set_update_interval(0);
  second.set_uart_parent(&uart);
  second.set_update_interval(0);
  second.polling_enabled = true;

  EXPECT_CALL(uart, write_array(::testing::_, 9)).Times(1);
  first.loop();

  EXPECT_EQ(first.poll_count, 0);
  EXPECT_EQ(second.poll_count, 1);
  EXPECT_TRUE(second.request_pending());
}

TEST(M5ChainBaseTest, StreamingInstanceDoesNotStarvePolling) {
  MockUART uart;
  StreamingBase streaming;
  TestBase poller;
  streaming.set_uart_parent(&uart);
  streaming.set_chain_index(1);
  streaming.set_update_interval(SCHEDULER_DONT_RUN);
  poller.set_uart_parent(&uart);
  poller.set_chain_index(2);
  poller.set_update_interval(0);
  poller.polling_enabled = true;

  EXPECT_CALL(uart, write_array(::testing::_, ::testing::_)).Times(::testing::AnyNumber());
  streaming.start_stream();

  // The response re-queues the next frame in the same tick, so the bus is never idle on its own.
  uart.inject(packet(1, 0x31, {0x01}));
  streaming.loop();
  EXPECT_TRUE(streaming.request_pending());

  std::this_thread::sleep_for(std::chrono::milliseconds(INTER_REQUEST_DELAY_MS + 2));
  streaming.loop();
  EXPECT_EQ(poller.poll_count, 1) << "the poll of the other instance must win over the re-queued frame";
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
