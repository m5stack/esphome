#pragma once

#include <cstdint>

#include "esphome/components/m5_chain_base/m5_chain_base.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::m5_chain_imu {

enum class ImuCommand : uint8_t {
  IMU_COMMAND_GET_ACCEL = 0x30,
  IMU_COMMAND_GET_GYRO = 0x31,
  IMU_COMMAND_GET_TEMPERATURE = 0x34,
  IMU_COMMAND_SET_ACCEL_RANGE = 0x35,
  IMU_COMMAND_SET_GYRO_RANGE = 0x37,
  IMU_COMMAND_SET_ACCEL_ODR = 0x39,
  IMU_COMMAND_SET_GYRO_ODR = 0x3B,
  IMU_COMMAND_EVENT_REPORT = 0xE0,
  IMU_COMMAND_SET_EVENT_MODE = 0xE1,
  IMU_COMMAND_SET_MOTION_THRESHOLD = 0xE3,
  IMU_COMMAND_SET_TEMPERATURE_THRESHOLD = 0xE5,
};

/// Bits of the event mode mask.
enum class ImuEventMode : uint8_t {
  IMU_EVENT_MODE_ACCEL = 0x01,
  IMU_EVENT_MODE_GYRO = 0x02,
  IMU_EVENT_MODE_TEMPERATURE = 0x04,
};

/// Work waiting for the bus. Lower values are sent first, so settings reach the device before the first read.
enum class ImuJob : uint8_t {
  IMU_JOB_ACCEL_RANGE = 0,
  IMU_JOB_GYRO_RANGE,
  IMU_JOB_ACCEL_ODR,
  IMU_JOB_GYRO_ODR,
  IMU_JOB_MOTION_THRESHOLD,
  IMU_JOB_TEMPERATURE_THRESHOLD,
  IMU_JOB_EVENT_MODE,
  IMU_JOB_RGB,
  IMU_JOB_BRIGHTNESS,
  IMU_JOB_READ_ACCEL,
  IMU_JOB_READ_GYRO,
  IMU_JOB_READ_TEMPERATURE,
  IMU_JOB_COUNT,
};

static constexpr uint8_t NOT_SET = 0xFF;

class M5ChainImu : public m5_chain_base::M5ChainBase {
  SUB_SENSOR(acceleration_x)
  SUB_SENSOR(acceleration_y)
  SUB_SENSOR(acceleration_z)
  SUB_SENSOR(gyroscope_x)
  SUB_SENSOR(gyroscope_y)
  SUB_SENSOR(gyroscope_z)
  SUB_SENSOR(temperature)
  SUB_BINARY_SENSOR(motion)
  SUB_BINARY_SENSOR(rotation)
  SUB_BINARY_SENSOR(temperature_alarm)

 public:
  void setup() override;
  void dump_config() override;

  /// Device range codes: accel 0 = 2G, 1 = 16G, 2 = 4G, 3 = 8G; gyro 0 = 250, 1 = 125, 2 = 500, 4 = 1000,
  /// 6 = 2000 dps.
  void set_accel_range(uint8_t code);
  void set_gyro_range(uint8_t code);
  /// Device output data rate codes, 0 = off up to 0x0A = 6.66 kHz (accel also 0x0B = 1.6 Hz).
  void set_accel_odr(uint8_t code);
  void set_gyro_odr(uint8_t code);
  /// Warning thresholds for the motion and rotation event reports, in device units.
  void set_motion_thresholds(uint8_t accel, uint8_t gyro);
  void enable_motion_event() { this->event_mode_ |= static_cast<uint8_t>(ImuEventMode::IMU_EVENT_MODE_ACCEL); }
  void enable_rotation_event() { this->event_mode_ |= static_cast<uint8_t>(ImuEventMode::IMU_EVENT_MODE_GYRO); }
  /// Temperature alarm thresholds in whole degrees Celsius.
  void set_temperature_alarm(uint8_t low, uint8_t high);

  m5_chain_base::ChainStatus set_rgb_color(uint8_t red, uint8_t green, uint8_t blue) override;
  m5_chain_base::ChainStatus set_rgb_color_component(uint8_t component, uint8_t value) override;
  m5_chain_base::ChainStatus set_led_brightness(uint8_t brightness) override;

 protected:
  void on_response(const m5_chain_base::ChainFrame &frame) override;
  void on_unsolicited(const m5_chain_base::ChainFrame &frame) override;
  void on_request_timeout(uint8_t index, uint8_t command) override;
  void poll_next_() override;

  void queue_job_(ImuJob job) { this->pending_jobs_ |= 1u << static_cast<uint8_t>(job); }
  void send_next_job_();
  bool handle_measurement_(const m5_chain_base::ChainFrame &frame);
  void handle_event_(const m5_chain_base::ChainFrame &frame);
  void mark_alive_();

  uint16_t pending_jobs_{0};
  /// NOT_SET leaves the device setting alone.
  uint8_t accel_range_{NOT_SET};
  uint8_t gyro_range_{NOT_SET};
  uint8_t accel_odr_{NOT_SET};
  uint8_t gyro_odr_{NOT_SET};
  uint8_t accel_threshold_{0};
  uint8_t gyro_threshold_{0};
  uint8_t temperature_low_{0};
  uint8_t temperature_high_{0};
  /// ImuEventMode bits of the alarms that have a binary sensor.
  uint8_t event_mode_{0};
  uint8_t rgb_red_{0};
  uint8_t rgb_green_{0};
  uint8_t rgb_blue_{0};
  uint8_t brightness_{0};
  /// The job whose request is on the wire, IMU_JOB_COUNT when none.
  ImuJob active_job_{ImuJob::IMU_JOB_COUNT};
  bool timeout_warning_active_{false};
};

}  // namespace esphome::m5_chain_imu
