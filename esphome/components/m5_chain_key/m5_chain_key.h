#pragma once

#include <cstdint>

#include "esphome/components/m5_chain_base/m5_chain_base.h"
#include "esphome/core/automation.h"

namespace esphome::binary_sensor {
class BinarySensor;
}

namespace esphome::m5_chain_key {

class M5ChainKey : public m5_chain_base::M5ChainBase {
 public:
  void setup() override;
  void dump_config() override;

  void set_key_sensor(binary_sensor::BinarySensor *sensor) { this->key_sensor_ = sensor; }

  /// Set the two onboard RGB LEDs. The Key device renders both LEDs identically.
  m5_chain_base::ChainStatus set_rgb_color(uint8_t red, uint8_t green, uint8_t blue) override;
  m5_chain_base::ChainStatus set_rgb_color_component(uint8_t component, uint8_t value) override;

  /// Set the onboard RGB brightness in percent without persisting it to flash.
  m5_chain_base::ChainStatus set_led_brightness(uint8_t brightness) override;

 protected:
  void on_response(const m5_chain_base::ChainFrame &frame) override;
  void on_unsolicited(const m5_chain_base::ChainFrame &frame) override;
  void on_request_timeout(uint8_t index, uint8_t command) override;

  void poll_key_();
  void handle_key_status_(const m5_chain_base::ChainFrame &frame);
  m5_chain_base::ChainStatus send_pending_rgb_();

  static constexpr uint32_t KEY_POLL_INTERVAL_ID = 0;
  static constexpr uint32_t KEY_RESPONSE_TIMEOUT_MS = 250;

  binary_sensor::BinarySensor *key_sensor_{nullptr};
  uint8_t rgb_red_{0};
  uint8_t rgb_green_{0};
  uint8_t rgb_blue_{0};
  bool rgb_update_pending_{false};
  bool timeout_warning_active_{false};
};

}  // namespace esphome::m5_chain_key
