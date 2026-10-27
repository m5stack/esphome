#pragma once

#include <array>
#include <cstdint>

#include "esphome/components/m5_chain_base/m5_chain_base.h"

namespace esphome::binary_sensor {
class BinarySensor;
}
namespace esphome::sensor {
class Sensor;
}

namespace esphome::m5_chain_joystick {

enum class JoystickCommand : uint8_t {
  JOYSTICK_COMMAND_GET_ADC_16 = 0x30,
  JOYSTICK_COMMAND_GET_ADC_8 = 0x31,
  JOYSTICK_COMMAND_GET_MAPPING_RANGE = 0x32,
  JOYSTICK_COMMAND_GET_MAPPED_16 = 0x34,
  JOYSTICK_COMMAND_GET_MAPPED_8 = 0x35,
  JOYSTICK_COMMAND_GET_BUTTON_STATUS = 0xE1,
};

enum class JoystickSensorType : uint8_t {
  JOYSTICK_SENSOR_ADC_16,
  JOYSTICK_SENSOR_ADC_8,
  JOYSTICK_SENSOR_MAPPED_16,
  JOYSTICK_SENSOR_MAPPED_8,
  JOYSTICK_SENSOR_MAPPING_RANGE,
};

class M5ChainJoystick : public m5_chain_base::M5ChainBase {
 public:
  void setup() override;
  void dump_config() override;

  void set_joystick_sensor(JoystickSensorType type, uint8_t axis, uint8_t range_index, sensor::Sensor *sensor);
  void set_button_sensor(binary_sensor::BinarySensor *sensor) { this->button_sensor_ = sensor; }

  m5_chain_base::ChainStatus set_rgb_color(uint8_t red, uint8_t green, uint8_t blue) override;
  m5_chain_base::ChainStatus set_rgb_color_component(uint8_t component, uint8_t value) override;
  m5_chain_base::ChainStatus set_led_brightness(uint8_t brightness) override;

 protected:
  void on_response(const m5_chain_base::ChainFrame &frame) override;
  void on_unsolicited(const m5_chain_base::ChainFrame &frame) override;
  void on_request_timeout(uint8_t index, uint8_t command) override;

  void poll_next_();
  void handle_response_(const m5_chain_base::ChainFrame &frame, bool continue_polling);
  void handle_button_status_(const m5_chain_base::ChainFrame &frame);
  bool has_sensor_type_(size_t type) const;
  m5_chain_base::ChainStatus send_pending_rgb_();

  static constexpr uint32_t POLL_INTERVAL_ID = 0;
  static constexpr uint32_t RESPONSE_TIMEOUT_MS = 250;
  static constexpr size_t SENSOR_TYPE_COUNT = 5;
  static constexpr size_t AXIS_COUNT = 2;

  std::array<std::array<sensor::Sensor *, AXIS_COUNT>, SENSOR_TYPE_COUNT> sensors_{};
  std::array<sensor::Sensor *, 8> mapping_range_sensors_{};
  binary_sensor::BinarySensor *button_sensor_{nullptr};
  uint8_t poll_slot_{0};
  uint8_t pending_type_{0};
  uint8_t rgb_red_{0};
  uint8_t rgb_green_{0};
  uint8_t rgb_blue_{0};
  bool rgb_update_pending_{false};
  bool timeout_warning_active_{false};
};

}  // namespace esphome::m5_chain_joystick
