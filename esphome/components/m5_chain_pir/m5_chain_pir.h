#pragma once

#include <cstdint>

#include "esphome/components/m5_chain_base/m5_chain_base.h"

namespace esphome::binary_sensor {
class BinarySensor;
}

namespace esphome::m5_chain_pir {

enum class PirCommand : uint8_t {
  PIR_COMMAND_GET_STATUS = 0x37,
  PIR_COMMAND_EVENT_REPORT = 0xE0,
  PIR_COMMAND_SET_EVENT_REPORT = 0xE1,
  PIR_COMMAND_GET_EVENT_REPORT = 0xE2,
  PIR_COMMAND_SET_KEEP_SECONDS = 0xE3,
  PIR_COMMAND_GET_KEEP_SECONDS = 0xE4,
};

class M5ChainPir : public m5_chain_base::M5ChainBase {
 public:
  void setup() override;
  void dump_config() override;

  void set_presence_sensor(binary_sensor::BinarySensor *sensor) { this->presence_sensor_ = sensor; }

  /// Turn the automatic detection event reports on or off.
  m5_chain_base::ChainStatus set_event_report(bool enabled);
  /// Hold time for the "person arrived" event, 0 to 255 seconds.
  m5_chain_base::ChainStatus set_keep_seconds(uint8_t seconds, bool save_to_flash);

  m5_chain_base::ChainStatus set_rgb_color(uint8_t red, uint8_t green, uint8_t blue) override;
  m5_chain_base::ChainStatus set_rgb_color_component(uint8_t component, uint8_t value) override;
  m5_chain_base::ChainStatus set_led_brightness(uint8_t brightness) override;

 protected:
  void on_response(const m5_chain_base::ChainFrame &frame) override;
  void on_unsolicited(const m5_chain_base::ChainFrame &frame) override;
  void on_request_timeout(uint8_t index, uint8_t command) override;

  void poll_next_() override;
  void publish_presence_(bool detected);
  m5_chain_base::ChainStatus send_pending_rgb_();

  binary_sensor::BinarySensor *presence_sensor_{nullptr};
  uint8_t rgb_red_{0};
  uint8_t rgb_green_{0};
  uint8_t rgb_blue_{0};
  bool rgb_update_pending_{false};
  bool timeout_warning_active_{false};
};

}  // namespace esphome::m5_chain_pir
