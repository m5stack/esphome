#pragma once

#include <cstdint>

#include "esphome/components/m5_chain_base/m5_chain_base.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::m5_chain_env {

enum class EnvCommand : uint8_t {
  ENV_COMMAND_GET_AHT20_DATA = 0x30,
  ENV_COMMAND_GET_SPA06_DATA = 0x31,
  ENV_COMMAND_SET_SEA_LEVEL_PRESSURE = 0x36,
  ENV_COMMAND_EVENT_REPORT = 0xE0,
  ENV_COMMAND_SET_EVENT_MODE = 0xE1,
  ENV_COMMAND_SET_TEMPERATURE_THRESHOLD = 0xE3,
  ENV_COMMAND_SET_HUMIDITY_THRESHOLD = 0xE5,
  ENV_COMMAND_SET_PRESSURE_THRESHOLD = 0xE7,
  ENV_COMMAND_SET_ALTITUDE_THRESHOLD = 0xE9,
};

/// Order of the alarms in the event mode mask (one bit each) and the event status byte (two bits each).
enum class EnvAlarm : uint8_t {
  ENV_ALARM_TEMPERATURE = 0,
  ENV_ALARM_HUMIDITY = 1,
  ENV_ALARM_PRESSURE = 2,
  ENV_ALARM_ALTITUDE = 3,
};

/// Work waiting for the bus. Lower values are sent first, so settings reach the device before the first read.
enum class EnvJob : uint8_t {
  ENV_JOB_SEA_LEVEL_PRESSURE = 0,
  ENV_JOB_TEMPERATURE_THRESHOLD,
  ENV_JOB_HUMIDITY_THRESHOLD,
  ENV_JOB_PRESSURE_THRESHOLD,
  ENV_JOB_ALTITUDE_THRESHOLD,
  ENV_JOB_EVENT_MODE,
  ENV_JOB_RGB,
  ENV_JOB_BRIGHTNESS,
  ENV_JOB_READ_AHT20,
  ENV_JOB_READ_SPA06,
  ENV_JOB_COUNT,
};

class M5ChainEnv : public m5_chain_base::M5ChainBase {
  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)
  SUB_SENSOR(pressure)
  SUB_SENSOR(altitude)
  SUB_SENSOR(barometer_temperature)
  SUB_BINARY_SENSOR(temperature_alarm)
  SUB_BINARY_SENSOR(humidity_alarm)
  SUB_BINARY_SENSOR(pressure_alarm)
  SUB_BINARY_SENSOR(altitude_alarm)

 public:
  void setup() override;
  void dump_config() override;

  /// Sea-level reference for the altitude calculation, in hPa x10 (3000 to 11000).
  void set_sea_level_pressure(uint16_t pressure_x10);
  /// Temperature alarm thresholds in whole degrees Celsius. `use_spa06` picks the barometer as the source.
  void set_temperature_alarm(uint8_t low, uint8_t high, bool use_spa06);
  /// Humidity alarm thresholds in percent.
  void set_humidity_alarm(uint8_t low, uint8_t high);
  /// Pressure alarm thresholds in hPa x10.
  void set_pressure_alarm(uint16_t low_x10, uint16_t high_x10);
  /// Altitude alarm thresholds in centimetres.
  void set_altitude_alarm(int32_t low_cm, int32_t high_cm);

  m5_chain_base::ChainStatus set_rgb_color(uint8_t red, uint8_t green, uint8_t blue) override;
  m5_chain_base::ChainStatus set_rgb_color_component(uint8_t component, uint8_t value) override;
  m5_chain_base::ChainStatus set_led_brightness(uint8_t brightness) override;

 protected:
  void on_response(const m5_chain_base::ChainFrame &frame) override;
  void on_unsolicited(const m5_chain_base::ChainFrame &frame) override;
  void on_request_timeout(uint8_t index, uint8_t command) override;
  void poll_next_() override;

  void queue_job_(EnvJob job);
  bool job_pending_(EnvJob job) const { return (this->pending_jobs_ & (1u << static_cast<uint8_t>(job))) != 0; }
  void send_next_job_();
  bool handle_measurement_(const m5_chain_base::ChainFrame &frame);
  void handle_event_(const m5_chain_base::ChainFrame &frame);
  void mark_alive_();

  int32_t altitude_low_cm_{0};
  int32_t altitude_high_cm_{0};
  uint16_t pressure_low_x10_{0};
  uint16_t pressure_high_x10_{0};
  uint16_t sea_level_pressure_x10_{0};
  uint16_t pending_jobs_{0};
  uint8_t temperature_low_{0};
  uint8_t temperature_high_{0};
  uint8_t humidity_low_{0};
  uint8_t humidity_high_{0};
  /// Bit per EnvAlarm: the alarms that have a binary sensor and should be reported by the device.
  uint8_t event_mode_{0};
  bool temperature_from_spa06_{false};
  uint8_t rgb_red_{0};
  uint8_t rgb_green_{0};
  uint8_t rgb_blue_{0};
  uint8_t brightness_{0};
  /// The job whose request is on the wire, ENV_JOB_COUNT when none.
  EnvJob active_job_{EnvJob::ENV_JOB_COUNT};
  bool timeout_warning_active_{false};
};

}  // namespace esphome::m5_chain_env
