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

namespace esphome::m5_chain_encoder {

enum class EncoderCommand : uint8_t {
  ENCODER_COMMAND_GET_VALUE = 0x10,
  ENCODER_COMMAND_GET_INCREMENT = 0x11,
  ENCODER_COMMAND_GET_AB_STATUS = 0x16,
  ENCODER_COMMAND_BUTTON_GET_STATUS = 0xE1,
};

enum class EncoderSensorType : uint8_t {
  ENCODER_SENSOR_VALUE,
  ENCODER_SENSOR_INCREMENT,
  ENCODER_SENSOR_AB_STATUS,
};

class M5ChainEncoder : public m5_chain_base::M5ChainBase {
 public:
  void setup() override;
  void dump_config() override;

  void set_encoder_sensor(EncoderSensorType type, sensor::Sensor *sensor);
  void set_button_sensor(binary_sensor::BinarySensor *sensor) { this->button_sensor_ = sensor; }

  m5_chain_base::ChainStatus set_rgb_color(uint8_t red, uint8_t green, uint8_t blue) override;
  m5_chain_base::ChainStatus set_rgb_color_component(uint8_t component, uint8_t value) override;
  m5_chain_base::ChainStatus set_led_brightness(uint8_t brightness) override;

 protected:
  void on_response(const m5_chain_base::ChainFrame &frame) override;
  void on_unsolicited(const m5_chain_base::ChainFrame &frame) override;
  void on_request_timeout(uint8_t index, uint8_t command) override;

  void poll_next_();
  m5_chain_base::ChainStatus send_pending_rgb_();

  static constexpr uint32_t POLL_INTERVAL_ID = 0;
  static constexpr uint32_t RESPONSE_TIMEOUT_MS = 150;
  static constexpr size_t SENSOR_SLOT_COUNT = 3;

  std::array<sensor::Sensor *, SENSOR_SLOT_COUNT> sensors_{};
  binary_sensor::BinarySensor *button_sensor_{nullptr};
  uint8_t poll_slot_{0};
  uint8_t pending_slot_{0};
  uint8_t rgb_red_{0};
  uint8_t rgb_green_{0};
  uint8_t rgb_blue_{0};
  bool rgb_update_pending_{false};
};

}  // namespace esphome::m5_chain_encoder
