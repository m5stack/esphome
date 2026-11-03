#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "esphome/components/m5_chain_base/m5_chain_base.h"

namespace esphome::sensor {
class Sensor;
}

namespace esphome::m5_chain_angle {

enum class AngleCommand : uint8_t {
  ANGLE_COMMAND_GET_ADC_12 = 0x30,
  ANGLE_COMMAND_GET_ADC_8 = 0x31,
  ANGLE_COMMAND_SET_DIRECTION = 0x32,
  ANGLE_COMMAND_GET_DIRECTION = 0x33,
};

enum class AngleSensorType : uint8_t {
  ANGLE_SENSOR_ADC_12,
  ANGLE_SENSOR_ADC_8,
};

class M5ChainAngle : public m5_chain_base::M5ChainBase {
 public:
  void setup() override;
  void dump_config() override;

  void set_angle_sensor(AngleSensorType type, sensor::Sensor *sensor);

  m5_chain_base::ChainStatus set_rgb_color(uint8_t red, uint8_t green, uint8_t blue) override;
  m5_chain_base::ChainStatus set_rgb_color_component(uint8_t component, uint8_t value) override;
  m5_chain_base::ChainStatus set_led_brightness(uint8_t brightness) override;

 protected:
  void on_response(const m5_chain_base::ChainFrame &frame) override;
  void on_unsolicited(const m5_chain_base::ChainFrame &frame) override;
  void on_request_timeout(uint8_t index, uint8_t command) override;

  void poll_next_() override;
  void handle_response_(const m5_chain_base::ChainFrame &frame, uint8_t slot, bool is_request_response);
  bool has_sensor_(size_t slot) const;
  m5_chain_base::ChainStatus send_pending_rgb_();

  static constexpr size_t SENSOR_SLOT_COUNT = 2;

  std::array<sensor::Sensor *, SENSOR_SLOT_COUNT> sensors_{};
  uint8_t rgb_red_{0};
  uint8_t rgb_green_{0};
  uint8_t rgb_blue_{0};
  bool rgb_update_pending_{false};
  bool timeout_warning_active_{false};
  uint8_t poll_slot_{0};
  uint8_t pending_slot_{0};
};

}  // namespace esphome::m5_chain_angle
