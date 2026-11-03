#pragma once

#include <cstdint>

#include "esphome/components/m5_chain_base/m5_chain_base.h"

namespace esphome::m5_chain_buzzer {

enum class BuzzerCommand : uint8_t {
  BUZZER_COMMAND_SET_MODE = 0x30,
  BUZZER_COMMAND_PLAY_AUTO = 0x32,
  BUZZER_COMMAND_SET_FREQUENCY = 0x33,
  BUZZER_COMMAND_SET_DUTY = 0x35,
  BUZZER_COMMAND_SET_STATUS = 0x37,
  BUZZER_COMMAND_PLAY_NOTE = 0x39,
};

enum class BuzzerMode : uint8_t {
  BUZZER_MODE_AUTO = 0,
  BUZZER_MODE_MANUAL = 1,
  BUZZER_MODE_NOTE = 2,
};

class M5ChainBuzzer : public m5_chain_base::M5ChainBase {
 public:
  void setup() override;
  void dump_config() override;

  m5_chain_base::ChainStatus set_mode(BuzzerMode mode);
  m5_chain_base::ChainStatus play_auto(uint16_t frequency, uint8_t duty, uint16_t duration_ms);
  m5_chain_base::ChainStatus set_frequency(uint16_t frequency);
  m5_chain_base::ChainStatus set_duty(uint8_t duty);
  m5_chain_base::ChainStatus set_status(bool enabled);
  m5_chain_base::ChainStatus play_note(uint8_t note, uint16_t duration_ms);

  m5_chain_base::ChainStatus set_rgb_color(uint8_t red, uint8_t green, uint8_t blue) override;
  m5_chain_base::ChainStatus set_rgb_color_component(uint8_t component, uint8_t value) override;
  m5_chain_base::ChainStatus set_led_brightness(uint8_t brightness) override;

 protected:
  void on_response(const m5_chain_base::ChainFrame &frame) override;
  void on_request_timeout(uint8_t index, uint8_t command) override;

  m5_chain_base::ChainStatus send_pending_rgb_();

  uint8_t rgb_red_{0};
  uint8_t rgb_green_{0};
  uint8_t rgb_blue_{0};
  bool rgb_update_pending_{false};
  bool timeout_warning_active_{false};
};

}  // namespace esphome::m5_chain_buzzer
