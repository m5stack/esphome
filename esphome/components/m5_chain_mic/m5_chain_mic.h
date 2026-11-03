#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "esphome/components/m5_chain_base/m5_chain_base.h"

namespace esphome::binary_sensor {
class BinarySensor;
}
namespace esphome::sensor {
class Sensor;
}

namespace esphome::m5_chain_mic {

/// Highest ADC value the 12-bit converter reports.
static constexpr uint16_t MIC_ADC_12_MAX = 4095;
/// Lowest and highest accepted trigger interval, in milliseconds.
static constexpr uint16_t MIC_TRIGGER_INTERVAL_MIN_MS = 300;
static constexpr uint16_t MIC_TRIGGER_INTERVAL_MAX_MS = 1000;

enum class MicCommand : uint8_t {
  MIC_COMMAND_GET_ADC_12 = 0x30,
  MIC_COMMAND_GET_ADC_8 = 0x31,
  MIC_COMMAND_SET_THRESHOLD = 0x32,
  MIC_COMMAND_GET_THRESHOLD = 0x33,
  MIC_COMMAND_TRIGGER_REPORT = 0xE0,
  MIC_COMMAND_SET_REPORT_MODE = 0xE1,
  MIC_COMMAND_GET_REPORT_MODE = 0xE2,
  MIC_COMMAND_SET_TRIGGER_INTERVAL = 0xE3,
  MIC_COMMAND_GET_TRIGGER_INTERVAL = 0xE4,
};

enum class MicSensorType : uint8_t {
  MIC_SENSOR_ADC_12,
  MIC_SENSOR_ADC_8,
};

class M5ChainMic : public m5_chain_base::M5ChainBase {
 public:
  void setup() override;
  void dump_config() override;

  void set_adc_sensor(MicSensorType type, sensor::Sensor *sensor);
  void set_trigger_sensor(binary_sensor::BinarySensor *sensor) { this->trigger_sensor_ = sensor; }

  /// Level the 12-bit ADC value has to cross to trigger a report, 0 to 4095.
  m5_chain_base::ChainStatus set_threshold(uint16_t threshold, bool save_to_flash);
  /// Turn the automatic threshold reports on or off.
  m5_chain_base::ChainStatus set_report_mode(bool enabled);
  /// Minimum time between two threshold reports, 300 to 1000 ms.
  m5_chain_base::ChainStatus set_trigger_interval(uint16_t interval_ms);

  m5_chain_base::ChainStatus set_rgb_color(uint8_t red, uint8_t green, uint8_t blue) override;
  m5_chain_base::ChainStatus set_rgb_color_component(uint8_t component, uint8_t value) override;
  m5_chain_base::ChainStatus set_led_brightness(uint8_t brightness) override;

 protected:
  void on_response(const m5_chain_base::ChainFrame &frame) override;
  void on_unsolicited(const m5_chain_base::ChainFrame &frame) override;
  void on_request_timeout(uint8_t index, uint8_t command) override;

  void poll_next_() override;
  void publish_adc_(const m5_chain_base::ChainFrame &frame, MicSensorType type);
  m5_chain_base::ChainStatus send_pending_rgb_();

  static constexpr size_t MIC_SENSOR_SLOT_COUNT = 2;

  std::array<sensor::Sensor *, MIC_SENSOR_SLOT_COUNT> sensors_{};
  binary_sensor::BinarySensor *trigger_sensor_{nullptr};
  uint8_t rgb_red_{0};
  uint8_t rgb_green_{0};
  uint8_t rgb_blue_{0};
  bool rgb_update_pending_{false};
  bool timeout_warning_active_{false};
  size_t poll_slot_{0};
};

}  // namespace esphome::m5_chain_mic
