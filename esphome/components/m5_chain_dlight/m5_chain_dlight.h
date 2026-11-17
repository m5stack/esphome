#pragma once

#include <cstdint>

#include "esphome/components/m5_chain_base/m5_chain_base.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::m5_chain_dlight {

enum class DLightCommand : uint8_t {
  DLIGHT_COMMAND_GET_LUX = 0x30,
  DLIGHT_COMMAND_SET_INTERRUPT_ENABLE = 0x31,
  DLIGHT_COMMAND_SET_INTERRUPT_THRESHOLD = 0x33,
  DLIGHT_COMMAND_SET_GAIN = 0x35,
  DLIGHT_COMMAND_SET_RESOLUTION = 0x37,
  DLIGHT_COMMAND_EVENT_REPORT = 0xE0,
  DLIGHT_COMMAND_SET_EVENT_MODE = 0xE1,
};

/// Status byte of a 0xE0 report.
enum class DLightEvent : uint8_t {
  DLIGHT_EVENT_LOW_ACTIVE = 0x00,
  DLIGHT_EVENT_HIGH_ACTIVE = 0x01,
  DLIGHT_EVENT_LOW_RELEASE = 0x02,
  DLIGHT_EVENT_HIGH_RELEASE = 0x03,
};

/// Work waiting for the bus. Lower values are sent first, so settings reach the device before the first read.
enum class DLightJob : uint8_t {
  DLIGHT_JOB_GAIN = 0,
  DLIGHT_JOB_RESOLUTION,
  DLIGHT_JOB_THRESHOLD,
  DLIGHT_JOB_INTERRUPT_ENABLE,
  DLIGHT_JOB_EVENT_MODE,
  DLIGHT_JOB_RGB,
  DLIGHT_JOB_BRIGHTNESS,
  DLIGHT_JOB_READ_LUX,
  DLIGHT_JOB_COUNT,
};

static constexpr uint8_t NOT_SET = 0xFF;
/// Largest lux threshold the device accepts.
static constexpr uint32_t DLIGHT_MAX_THRESHOLD = 2516580;

class M5ChainDLight : public m5_chain_base::M5ChainBase {
  SUB_SENSOR(illuminance)
  SUB_BINARY_SENSOR(low_light)
  SUB_BINARY_SENSOR(high_light)

 public:
  void setup() override;
  void dump_config() override;

  /// LTR-308 gain code: 0 = 1x, 1 = 3x, 2 = 6x, 3 = 9x, 4 = 18x.
  void set_gain(uint8_t gain);
  /// LTR-308 resolution code: 0 = 20 bit 400 ms, 1 = 19 bit 200 ms, 2 = 18 bit 100 ms, 3 = 17 bit 50 ms,
  /// 4 = 16 bit 25 ms.
  void set_resolution(uint8_t resolution);
  /// Lux band for the event reports. Below `low` turns the low light alarm on, above `high` the high light one.
  void set_thresholds(uint32_t low, uint32_t high);

  m5_chain_base::ChainStatus set_rgb_color(uint8_t red, uint8_t green, uint8_t blue) override;
  m5_chain_base::ChainStatus set_rgb_color_component(uint8_t component, uint8_t value) override;
  m5_chain_base::ChainStatus set_led_brightness(uint8_t brightness) override;

 protected:
  void on_response(const m5_chain_base::ChainFrame &frame) override;
  void on_unsolicited(const m5_chain_base::ChainFrame &frame) override;
  void on_request_timeout(uint8_t index, uint8_t command) override;
  void poll_next_() override;

  void queue_job_(DLightJob job) { this->pending_jobs_ |= 1u << static_cast<uint8_t>(job); }
  void send_next_job_();
  bool handle_lux_(const m5_chain_base::ChainFrame &frame);
  void handle_event_(const m5_chain_base::ChainFrame &frame);
  void mark_alive_();

  uint32_t threshold_low_{0};
  uint32_t threshold_high_{DLIGHT_MAX_THRESHOLD};
  uint16_t pending_jobs_{0};
  /// NOT_SET leaves the device setting alone.
  uint8_t gain_{NOT_SET};
  uint8_t resolution_{NOT_SET};
  bool events_enabled_{false};
  uint8_t rgb_red_{0};
  uint8_t rgb_green_{0};
  uint8_t rgb_blue_{0};
  uint8_t brightness_{0};
  /// The job whose request is on the wire, DLIGHT_JOB_COUNT when none.
  DLightJob active_job_{DLightJob::DLIGHT_JOB_COUNT};
  bool timeout_warning_active_{false};
};

}  // namespace esphome::m5_chain_dlight
