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

  /// Hold a tone until set_tone_off() is called. This is what a level-based
  /// float output (the rtttl component) needs: the module's auto-play command
  /// is one-shot and stops once its duration elapses, so the tone is started
  /// with the longest duration and replaced by the next call.
  void set_tone(uint16_t frequency, uint8_t duty);
  void set_tone_off();

  m5_chain_base::ChainStatus set_rgb_color(uint8_t red, uint8_t green, uint8_t blue) override;
  m5_chain_base::ChainStatus set_rgb_color_component(uint8_t component, uint8_t value) override;
  m5_chain_base::ChainStatus set_led_brightness(uint8_t brightness) override;

 protected:
  void on_response(const m5_chain_base::ChainFrame &frame) override;
  void on_request_timeout(uint8_t index, uint8_t command) override;
  void poll_next_() override;

  m5_chain_base::ChainStatus send_pending_rgb_();
  void service_tone_();

  uint8_t rgb_red_{0};
  uint8_t rgb_green_{0};
  uint8_t rgb_blue_{0};
  bool rgb_update_pending_{false};
  bool tone_active_{false};
  bool tone_pending_{false};
  uint16_t tone_frequency_{2700};
  uint8_t tone_duty_{50};
  bool timeout_warning_active_{false};
};

}  // namespace esphome::m5_chain_buzzer
