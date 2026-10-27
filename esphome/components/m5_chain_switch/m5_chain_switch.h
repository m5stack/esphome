#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "esphome/components/m5_chain_base/m5_chain_base.h"

namespace esphome::switch_ {
class Switch;
}
namespace esphome::binary_sensor {
class BinarySensor;
}
namespace esphome::sensor {
class Sensor;
}

namespace esphome::m5_chain_switch {

enum class SwitchCommand : uint8_t {
  SWITCH_COMMAND_GET_ADC_12 = 0x30,
  SWITCH_COMMAND_GET_ADC_8 = 0x31,
  SWITCH_COMMAND_GET_CHANGE_MODE = 0x33,
  SWITCH_COMMAND_GET_THRESHOLD = 0x35,
  SWITCH_COMMAND_GET_STATUS = 0x36,
  SWITCH_COMMAND_SET_EVENT_TRIGGER = 0xE1,
  SWITCH_COMMAND_GET_EVENT_TRIGGER = 0xE2,
};

enum class SwitchSensorType : uint8_t {
  SWITCH_SENSOR_ADC_12,
  SWITCH_SENSOR_ADC_8,
  SWITCH_SENSOR_CHANGE_MODE,
  SWITCH_SENSOR_OPEN_THRESHOLD,
  SWITCH_SENSOR_CLOSE_THRESHOLD,
};

class M5ChainSwitch : public m5_chain_base::M5ChainBase {
 public:
  void setup() override;
  void dump_config() override;

  void set_event_trigger_switch(switch_::Switch *switch_entity) { this->event_trigger_switch_ = switch_entity; }
  void set_switch_sensor(SwitchSensorType type, sensor::Sensor *sensor);
  void set_status_sensor(binary_sensor::BinarySensor *sensor) { this->status_sensor_ = sensor; }

  m5_chain_base::ChainStatus set_rgb_color(uint8_t red, uint8_t green, uint8_t blue) override;
  m5_chain_base::ChainStatus set_rgb_color_component(uint8_t component, uint8_t value) override;
  m5_chain_base::ChainStatus set_led_brightness(uint8_t brightness) override;
  m5_chain_base::ChainStatus set_event_trigger(bool enabled);

 protected:
  void on_response(const m5_chain_base::ChainFrame &frame) override;
  void on_unsolicited(const m5_chain_base::ChainFrame &frame) override;
  void on_request_timeout(uint8_t index, uint8_t command) override;

  void poll_next_();
  void handle_response_(const m5_chain_base::ChainFrame &frame, uint8_t slot, bool continue_polling);
  void handle_status_report_(const m5_chain_base::ChainFrame &frame);
  bool has_sensor_(size_t slot) const;
  m5_chain_base::ChainStatus send_pending_rgb_();

  static constexpr uint32_t EVENT_TRIGGER_POLL_INTERVAL_ID = 0;
  static constexpr size_t SENSOR_SLOT_COUNT = 5;

  std::array<sensor::Sensor *, SENSOR_SLOT_COUNT> sensors_{};
  switch_::Switch *event_trigger_switch_{nullptr};
  binary_sensor::BinarySensor *status_sensor_{nullptr};
  uint8_t rgb_red_{0};
  uint8_t rgb_green_{0};
  uint8_t rgb_blue_{0};
  bool rgb_update_pending_{false};
  bool event_trigger_state_pending_{false};
  bool timeout_warning_active_{false};
  uint8_t poll_slot_{0};
  uint8_t pending_slot_{0};
};

}  // namespace esphome::m5_chain_switch
