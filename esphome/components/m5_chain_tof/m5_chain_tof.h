#pragma once

#include <cstdint>

#include "esphome/components/m5_chain_base/m5_chain_base.h"

namespace esphome::sensor {
class Sensor;
}

namespace esphome::m5_chain_tof {

enum class TofCommand : uint8_t {
  TOF_COMMAND_GET_DISTANCE = 0x50,
  TOF_COMMAND_SET_MEASUREMENT_TIME = 0x51,
  TOF_COMMAND_GET_MEASUREMENT_TIME = 0x52,
  TOF_COMMAND_SET_MEASUREMENT_MODE = 0x53,
  TOF_COMMAND_GET_MEASUREMENT_MODE = 0x54,
  TOF_COMMAND_SET_MEASUREMENT_STATUS = 0x55,
  TOF_COMMAND_GET_MEASUREMENT_STATUS = 0x56,
  TOF_COMMAND_GET_MEASUREMENT_FLAG = 0x57,
};

class M5ChainToF : public m5_chain_base::M5ChainBase {
 public:
  void setup() override;
  void dump_config() override;

  void set_distance_sensor(sensor::Sensor *sensor) { this->distance_sensor_ = sensor; }

  m5_chain_base::ChainStatus set_rgb_color(uint8_t red, uint8_t green, uint8_t blue) override;
  m5_chain_base::ChainStatus set_rgb_color_component(uint8_t component, uint8_t value) override;
  m5_chain_base::ChainStatus set_led_brightness(uint8_t brightness) override;

 protected:
  void on_response(const m5_chain_base::ChainFrame &frame) override;
  void on_unsolicited(const m5_chain_base::ChainFrame &frame) override;
  void on_request_timeout(uint8_t index, uint8_t command) override;

  void poll_distance_();
  void handle_distance_(const m5_chain_base::ChainFrame &frame);
  m5_chain_base::ChainStatus send_pending_rgb_();

  static constexpr uint32_t TOF_POLL_INTERVAL_ID = 0;
  static constexpr uint32_t TOF_RESPONSE_TIMEOUT_MS = 250;

  sensor::Sensor *distance_sensor_{nullptr};
  uint8_t rgb_red_{0};
  uint8_t rgb_green_{0};
  uint8_t rgb_blue_{0};
  bool rgb_update_pending_{false};
  bool timeout_warning_active_{false};
};

}  // namespace esphome::m5_chain_tof
