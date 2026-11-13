#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "esphome/components/light/light_state.h"
#include "esphome/components/m5_chain_mono/light/m5_chain_mono_light.h"
#include "esphome/components/m5_chain_mono/m5_chain_mono.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::m5_chain_mono::testing {
namespace {

class MockUART : public uart::UARTComponent {
 public:
  size_t available() override { return 0; }
  bool read_array(uint8_t *, size_t) override { return false; }
  bool peek_byte(uint8_t *) override { return false; }
  MOCK_METHOD(void, write_array, (const uint8_t *data, size_t size), (override));
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS; }
  void check_logger_conflict() override {}
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool) override {}
#endif
};

class TestLight : public M5ChainMonoLight {
 public:
  using M5ChainMonoLight::screen_brightness_level_;
};

static void wait_for_next_bus_slot() {
  std::this_thread::sleep_for(std::chrono::milliseconds(m5_chain_base::INTER_REQUEST_DELAY_MS + 2));
}

// The light state restores its colour during its own setup, which runs at HARDWARE - 1 and writes
// through get_view_internal(). The pixel buffer therefore has to be allocated before that, or the
// first boot ends in a store to a null pointer.
TEST(M5ChainMonoLightTest, SetsUpBeforeTheLightState) {
  M5ChainMonoLight light;
  EXPECT_GT(light.get_setup_priority(), setup_priority::HARDWARE - 1.0f);
}

TEST(M5ChainMonoLightTest, CoversTheWholePanel) {
  M5ChainMonoLight light;
  EXPECT_EQ(light.size(), 64);
}

// One bit per pixel means the panel cannot show a colour: it dims, and Home Assistant has to offer
// a brightness slider instead of a colour wheel.
TEST(M5ChainMonoLightTest, DimsInsteadOfColouring) {
  M5ChainMonoLight light;
  const auto modes = light.get_traits().get_supported_color_modes();
  EXPECT_EQ(modes.size(), 1u);
  EXPECT_TRUE(modes.count(light::ColorMode::BRIGHTNESS));
  EXPECT_FALSE(modes.count(light::ColorMode::RGB));
}

TEST(M5ChainMonoLightTest, MapsBrightnessOntoTheScreenLevels) {
  EXPECT_EQ(TestLight::screen_brightness_level_(0.0f), 0);
  EXPECT_EQ(TestLight::screen_brightness_level_(0.01f), 1);  // a lit light never leaves the panel dark
  EXPECT_EQ(TestLight::screen_brightness_level_(0.5f), 4);
  EXPECT_EQ(TestLight::screen_brightness_level_(1.0f), MAX_SCREEN_BRIGHTNESS_LEVEL);
}

TEST(M5ChainMonoLightTest, SendsTheScreenBrightnessBeforeTheFrame) {
  MockUART uart;
  M5ChainMono mono;
  mono.set_uart_parent(&uart);
  mono.set_chain_index(1);

  M5ChainMonoLight light;
  light.set_parent(&mono);
  light.setup();

  light::LightState state(&light);
  state.current_values.set_color_mode(light::ColorMode::BRIGHTNESS);
  state.current_values.set_state(true);
  state.current_values.set_brightness(0.5f);

  std::vector<std::vector<uint8_t>> frames;
  EXPECT_CALL(uart, write_array(::testing::_, ::testing::_))
      .WillRepeatedly([&frames](const uint8_t *data, size_t size) { frames.emplace_back(data, data + size); });

  wait_for_next_bus_slot();
  light.write_state(&state);
  mono.loop();

  ASSERT_FALSE(frames.empty());
  EXPECT_EQ(frames[0][5], static_cast<uint8_t>(MonoCommand::MONO_COMMAND_SET_BRIGHTNESS));
  EXPECT_EQ(frames[0][6], 4) << "half brightness is level 4 of 7";
  EXPECT_EQ(frames[0][7], 0) << "the level must not be written to the module's flash";
}

}  // namespace
}  // namespace esphome::m5_chain_mono::testing
