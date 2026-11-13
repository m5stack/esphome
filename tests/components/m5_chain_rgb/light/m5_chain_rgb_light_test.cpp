#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "esphome/components/light/light_state.h"
#include "esphome/components/m5_chain_rgb/light/m5_chain_rgb_light.h"
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

// The light state restores its colour during its own setup, which runs at HARDWARE - 1 and writes
// through get_view_internal(). The pixel buffer therefore has to be allocated before that, or the
// first boot ends in a store to a null pointer.
TEST(M5ChainRGBLightTest, SetsUpBeforeTheLightState) {
  M5ChainRGBLight light;
  EXPECT_GT(light.get_setup_priority(), setup_priority::HARDWARE - 1.0f);
}

TEST(M5ChainRGBLightTest, CoversTheWholePanel) {
  M5ChainRGBLight light;
  EXPECT_EQ(light.size(), 64);
}

TEST(M5ChainRGBLightTest, PinsTheModuleBrightnessBeforeTheFrame) {
  MockUART uart;
  M5ChainRGB rgb;
  rgb.set_uart_parent(&uart);
  rgb.set_chain_index(1);

  M5ChainRGBLight light;
  light.set_parent(&rgb);
  light.setup();

  light::LightState state(&light);
  state.current_values.set_color_mode(light::ColorMode::RGB);
  state.current_values.set_state(true);
  state.current_values.set_brightness(1.0f);

  std::vector<std::vector<uint8_t>> frames;
  EXPECT_CALL(uart, write_array(::testing::_, ::testing::_))
      .WillRepeatedly([&frames](const uint8_t *data, size_t size) { frames.emplace_back(data, data + size); });

  wait_for_next_bus_slot();
  light.write_state(&state);
  rgb.loop();

  ASSERT_FALSE(frames.empty());
  EXPECT_EQ(frames[0][5], static_cast<uint8_t>(RgbCommand::RGB_COMMAND_SET_BRIGHTNESS));
  EXPECT_EQ(frames[0][6], MAX_SCREEN_BRIGHTNESS) << "the panel keeps its full range";
  EXPECT_EQ(frames[0][7], 0) << "the value must not be written to the module's flash";

  uart.inject(packet(1, static_cast<uint8_t>(RgbCommand::RGB_COMMAND_SET_BRIGHTNESS), {1}));
  rgb.loop();
  wait_for_the_frame_to_clear();
  rgb.loop();

  ASSERT_EQ(frames.size(), 2u);
  EXPECT_EQ(frames[1][5], static_cast<uint8_t>(RgbCommand::RGB_COMMAND_SET_DISPLAY_BUFFER));
  EXPECT_FALSE(light.status_has_warning());
}

TEST(M5ChainRGBLightTest, SendsTheBrightnessOnlyOnce) {
  MockUART uart;
  M5ChainRGB rgb;
  rgb.set_uart_parent(&uart);
  rgb.set_chain_index(1);

  M5ChainRGBLight light;
  light.set_parent(&rgb);
  light.setup();

  light::LightState state(&light);
  state.current_values.set_color_mode(light::ColorMode::RGB);
  state.current_values.set_state(true);
  state.current_values.set_brightness(1.0f);

  std::vector<std::vector<uint8_t>> frames;
  EXPECT_CALL(uart, write_array(::testing::_, ::testing::_))
      .WillRepeatedly([&frames](const uint8_t *data, size_t size) { frames.emplace_back(data, data + size); });

  wait_for_next_bus_slot();
  light.write_state(&state);
  rgb.loop();
  uart.inject(packet(1, static_cast<uint8_t>(RgbCommand::RGB_COMMAND_SET_BRIGHTNESS), {1}));
  rgb.loop();
  wait_for_the_frame_to_clear();
  rgb.loop();
  const size_t after_first_write = frames.size();

  // Every later frame skips the brightness: the panel already runs at the pinned level.
  wait_for_the_frame_to_clear();
  light.write_state(&state);
  rgb.loop();
  wait_for_the_frame_to_clear();
  rgb.loop();

  size_t brightness_requests = 0;
  for (const auto &frame : frames)
    brightness_requests += frame[5] == static_cast<uint8_t>(RgbCommand::RGB_COMMAND_SET_BRIGHTNESS);
  EXPECT_EQ(brightness_requests, 1u);
  EXPECT_GT(frames.size(), after_first_write) << "the frames themselves keep going out";
}

}  // namespace
}  // namespace esphome::m5_chain_rgb::testing
