#include "m5_chain_imu.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::m5_chain_imu {

ESPHOME_LOG_TAG(TAG, "m5_chain_imu");

using m5_chain_base::ChainCommand;
using m5_chain_base::ChainFrame;
using m5_chain_base::ChainStatus;

static constexpr uint8_t cmd(ImuCommand command) { return static_cast<uint8_t>(command); }
static constexpr uint8_t cmd(ChainCommand command) { return static_cast<uint8_t>(command); }

static constexpr uint8_t OPERATION_SUCCESS = 1;
/// Second byte of a 0xE0 report: the device type code of the Chain IMU.
static constexpr uint8_t IMU_EVENT_REPORT_TYPE = 0x10;
static constexpr uint16_t AXES_DATA_SIZE = 6;
static constexpr uint16_t TEMPERATURE_DATA_SIZE = 2;

/// True while a configured alarm still waits for the reading that gives it its first state.
static bool awaits_initial_state(binary_sensor::BinarySensor *alarm) { return alarm != nullptr && !alarm->has_state(); }

/// Give an alarm its first state from a reading; once it has one, only the device's reports change it.
static void publish_initial_alarm(binary_sensor::BinarySensor *alarm, bool active) {
  if (alarm != nullptr && !alarm->has_state())
    alarm->publish_state(active);
}
static constexpr float MILLI_G_TO_M_S2 = 9.80665f / 1000.0f;
/// Temperature field of the event status byte: high and low threshold crossings.
static constexpr uint8_t EVENT_TEMPERATURE_MASK = 0x0C;
static constexpr uint8_t EVENT_TEMPERATURE_HIGH_ACTIVE = 0x04;
static constexpr uint8_t EVENT_TEMPERATURE_LOW_ACTIVE = 0x08;

void M5ChainImu::setup() {
  // Always send the event mode: alarms without a binary sensor are switched off.
  this->queue_job_(ImuJob::IMU_JOB_EVENT_MODE);
  this->send_next_job_();
}

void M5ChainImu::dump_config() {
  this->dump_base_config();
  ESP_LOGCONFIG(TAG,
                "M5Stack Chain IMU:\n"
                "  Chain index: %u",
                this->get_chain_index());
  if (this->accel_range_ != NOT_SET) {
    ESP_LOGCONFIG(TAG, "  Accelerometer range code: %u", this->accel_range_);
  }
  if (this->gyro_range_ != NOT_SET) {
    ESP_LOGCONFIG(TAG, "  Gyroscope range code: %u", this->gyro_range_);
  }
  if (this->accel_odr_ != NOT_SET) {
    ESP_LOGCONFIG(TAG, "  Accelerometer data rate code: %u", this->accel_odr_);
  }
  if (this->gyro_odr_ != NOT_SET) {
    ESP_LOGCONFIG(TAG, "  Gyroscope data rate code: %u", this->gyro_odr_);
  }
  LOG_SENSOR("  ", "Acceleration X", this->acceleration_x_sensor_);
  LOG_SENSOR("  ", "Acceleration Y", this->acceleration_y_sensor_);
  LOG_SENSOR("  ", "Acceleration Z", this->acceleration_z_sensor_);
  LOG_SENSOR("  ", "Gyroscope X", this->gyroscope_x_sensor_);
  LOG_SENSOR("  ", "Gyroscope Y", this->gyroscope_y_sensor_);
  LOG_SENSOR("  ", "Gyroscope Z", this->gyroscope_z_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_BINARY_SENSOR("  ", "Motion", this->motion_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Rotation", this->rotation_binary_sensor_);
  if (this->motion_binary_sensor_ != nullptr || this->rotation_binary_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "    Thresholds: accel %u, gyro %u", this->accel_threshold_, this->gyro_threshold_);
  }
  LOG_BINARY_SENSOR("  ", "Temperature alarm", this->temperature_alarm_binary_sensor_);
  if (this->temperature_alarm_binary_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "    Thresholds: %u to %u C", this->temperature_low_, this->temperature_high_);
  }
}

void M5ChainImu::set_accel_range(uint8_t code) {
  this->accel_range_ = code;
  this->queue_job_(ImuJob::IMU_JOB_ACCEL_RANGE);
}

void M5ChainImu::set_gyro_range(uint8_t code) {
  this->gyro_range_ = code;
  this->queue_job_(ImuJob::IMU_JOB_GYRO_RANGE);
}

void M5ChainImu::set_accel_odr(uint8_t code) {
  this->accel_odr_ = code;
  this->queue_job_(ImuJob::IMU_JOB_ACCEL_ODR);
}

void M5ChainImu::set_gyro_odr(uint8_t code) {
  this->gyro_odr_ = code;
  this->queue_job_(ImuJob::IMU_JOB_GYRO_ODR);
}

void M5ChainImu::set_motion_thresholds(uint8_t accel, uint8_t gyro) {
  this->accel_threshold_ = accel;
  this->gyro_threshold_ = gyro;
  this->queue_job_(ImuJob::IMU_JOB_MOTION_THRESHOLD);
}

void M5ChainImu::set_temperature_alarm(uint8_t low, uint8_t high) {
  this->temperature_low_ = low;
  this->temperature_high_ = high;
  this->event_mode_ |= static_cast<uint8_t>(ImuEventMode::IMU_EVENT_MODE_TEMPERATURE);
  this->queue_job_(ImuJob::IMU_JOB_TEMPERATURE_THRESHOLD);
}

ChainStatus M5ChainImu::set_rgb_color(uint8_t red, uint8_t green, uint8_t blue) {
  this->rgb_red_ = red;
  this->rgb_green_ = green;
  this->rgb_blue_ = blue;
  this->queue_job_(ImuJob::IMU_JOB_RGB);
  this->send_next_job_();
  return ChainStatus::CHAIN_STATUS_OK;
}

ChainStatus M5ChainImu::set_rgb_color_component(uint8_t component, uint8_t value) {
  if (component > 2)
    return ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  uint8_t *channels[] = {&this->rgb_red_, &this->rgb_green_, &this->rgb_blue_};
  *channels[component] = value;
  this->queue_job_(ImuJob::IMU_JOB_RGB);
  this->send_next_job_();
  return ChainStatus::CHAIN_STATUS_OK;
}

ChainStatus M5ChainImu::set_led_brightness(uint8_t brightness) {
  if (brightness > 100)
    return ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  this->brightness_ = brightness;
  this->queue_job_(ImuJob::IMU_JOB_BRIGHTNESS);
  this->send_next_job_();
  return ChainStatus::CHAIN_STATUS_OK;
}

void M5ChainImu::poll_next_() {
  // An alarm with no sensor of its own still needs one reading for its first state.
  if (this->acceleration_x_sensor_ != nullptr || this->acceleration_y_sensor_ != nullptr ||
      this->acceleration_z_sensor_ != nullptr || awaits_initial_state(this->motion_binary_sensor_) ||
      awaits_initial_state(this->rotation_binary_sensor_))
    this->queue_job_(ImuJob::IMU_JOB_READ_ACCEL);
  if (this->gyroscope_x_sensor_ != nullptr || this->gyroscope_y_sensor_ != nullptr ||
      this->gyroscope_z_sensor_ != nullptr)
    this->queue_job_(ImuJob::IMU_JOB_READ_GYRO);
  if (this->temperature_sensor_ != nullptr || awaits_initial_state(this->temperature_alarm_binary_sensor_))
    this->queue_job_(ImuJob::IMU_JOB_READ_TEMPERATURE);
  this->send_next_job_();
}

void M5ChainImu::send_next_job_() {
  if (this->pending_jobs_ == 0 || this->request_pending())
    return;

  ImuJob job = ImuJob::IMU_JOB_COUNT;
  for (uint8_t i = 0; i < static_cast<uint8_t>(ImuJob::IMU_JOB_COUNT); i++) {
    if (this->pending_jobs_ & (1u << i)) {
      job = static_cast<ImuJob>(i);
      break;
    }
  }

  uint8_t data[5];
  uint16_t size = 0;
  uint8_t command = 0;
  switch (job) {
    case ImuJob::IMU_JOB_ACCEL_RANGE:
      command = cmd(ImuCommand::IMU_COMMAND_SET_ACCEL_RANGE);
      data[0] = this->accel_range_;
      size = 1;
      break;
    case ImuJob::IMU_JOB_GYRO_RANGE:
      command = cmd(ImuCommand::IMU_COMMAND_SET_GYRO_RANGE);
      data[0] = this->gyro_range_;
      size = 1;
      break;
    case ImuJob::IMU_JOB_ACCEL_ODR:
      command = cmd(ImuCommand::IMU_COMMAND_SET_ACCEL_ODR);
      data[0] = this->accel_odr_;
      size = 1;
      break;
    case ImuJob::IMU_JOB_GYRO_ODR:
      command = cmd(ImuCommand::IMU_COMMAND_SET_GYRO_ODR);
      data[0] = this->gyro_odr_;
      size = 1;
      break;
    case ImuJob::IMU_JOB_MOTION_THRESHOLD:
      command = cmd(ImuCommand::IMU_COMMAND_SET_MOTION_THRESHOLD);
      data[0] = this->accel_threshold_;
      data[1] = this->gyro_threshold_;
      size = 2;
      break;
    case ImuJob::IMU_JOB_TEMPERATURE_THRESHOLD:
      command = cmd(ImuCommand::IMU_COMMAND_SET_TEMPERATURE_THRESHOLD);
      data[0] = this->temperature_low_;
      data[1] = this->temperature_high_;
      size = 2;
      break;
    case ImuJob::IMU_JOB_EVENT_MODE:
      command = cmd(ImuCommand::IMU_COMMAND_SET_EVENT_MODE);
      data[0] = this->event_mode_;
      size = 1;
      break;
    case ImuJob::IMU_JOB_RGB:
      command = cmd(ChainCommand::CHAIN_COMMAND_SET_RGB_VALUE);
      data[0] = 0;  // first LED
      data[1] = 1;  // one LED
      data[2] = this->rgb_red_;
      data[3] = this->rgb_green_;
      data[4] = this->rgb_blue_;
      size = 5;
      break;
    case ImuJob::IMU_JOB_BRIGHTNESS:
      command = cmd(ChainCommand::CHAIN_COMMAND_SET_RGB_LIGHT);
      data[0] = this->brightness_;
      data[1] = 0;  // do not save to flash
      size = 2;
      break;
    case ImuJob::IMU_JOB_READ_ACCEL:
      command = cmd(ImuCommand::IMU_COMMAND_GET_ACCEL);
      break;
    case ImuJob::IMU_JOB_READ_GYRO:
      command = cmd(ImuCommand::IMU_COMMAND_GET_GYRO);
      break;
    case ImuJob::IMU_JOB_READ_TEMPERATURE:
      command = cmd(ImuCommand::IMU_COMMAND_GET_TEMPERATURE);
      break;
    case ImuJob::IMU_JOB_COUNT:
      return;
  }

  const auto status = this->request(this->get_chain_index(), command, data, size);
  if (status == ChainStatus::CHAIN_STATUS_BUSY)
    return;
  // Clear the job when it leaves, not when it is answered: a new value queued while this one is on the
  // wire must be sent again afterwards.
  this->pending_jobs_ &= ~(1u << static_cast<uint8_t>(job));
  if (status == ChainStatus::CHAIN_STATUS_OK) {
    this->active_job_ = job;
  } else {
    ESP_LOGW(TAG, "Could not queue command 0x%02X (status=%u)", command, static_cast<uint8_t>(status));
    this->status_set_warning();
  }
}

void M5ChainImu::on_response(const ChainFrame &frame) {
  this->active_job_ = ImuJob::IMU_JOB_COUNT;
  if (!this->handle_measurement_(frame)) {
    // Every other request is a setting: its answer is one operation status byte.
    if (frame.data_size < 1 || frame.data[0] != OPERATION_SUCCESS) {
      ESP_LOGW(TAG, "Chain IMU rejected command 0x%02X", frame.command);
      this->status_set_warning();
    } else {
      this->mark_alive_();
    }
  }
  this->send_next_job_();
}

void M5ChainImu::on_unsolicited(const ChainFrame &frame) {
  if (frame.index != this->get_chain_index())
    return;
  if (frame.command == cmd(ImuCommand::IMU_COMMAND_EVENT_REPORT)) {
    this->handle_event_(frame);
    return;
  }
  // A reading can arrive after the request timed out when it crosses several chain nodes.
  this->handle_measurement_(frame);
}

bool M5ChainImu::handle_measurement_(const ChainFrame &frame) {
  const bool is_accel = frame.command == cmd(ImuCommand::IMU_COMMAND_GET_ACCEL);
  const bool is_gyro = frame.command == cmd(ImuCommand::IMU_COMMAND_GET_GYRO);
  if (is_accel || is_gyro) {
    if (frame.data_size < AXES_DATA_SIZE) {
      this->status_set_warning();
      return true;
    }
    // Acceleration arrives in mg, angular rate in whole degrees per second.
    const float scale = is_accel ? MILLI_G_TO_M_S2 : 1.0f;
    sensor::Sensor *axes[3];
    if (is_accel) {
      axes[0] = this->acceleration_x_sensor_;
      axes[1] = this->acceleration_y_sensor_;
      axes[2] = this->acceleration_z_sensor_;
    } else {
      axes[0] = this->gyroscope_x_sensor_;
      axes[1] = this->gyroscope_y_sensor_;
      axes[2] = this->gyroscope_z_sensor_;
    }
    for (uint8_t i = 0; i < 3; i++) {
      if (axes[i] != nullptr)
        axes[i]->publish_state(encode_value_le<int16_t>(frame.data + i * 2) * scale);
    }
    // The device only reports movement, so a moving state cannot be read back: start from still.
    publish_initial_alarm(this->motion_binary_sensor_, false);
    publish_initial_alarm(this->rotation_binary_sensor_, false);
    this->mark_alive_();
    return true;
  }

  if (frame.command == cmd(ImuCommand::IMU_COMMAND_GET_TEMPERATURE)) {
    if (frame.data_size < TEMPERATURE_DATA_SIZE) {
      this->status_set_warning();
      return true;
    }
    const float temperature = encode_value_le<int16_t>(frame.data) / 100.0f;
    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(temperature);
    publish_initial_alarm(this->temperature_alarm_binary_sensor_,
                          temperature < this->temperature_low_ || temperature > this->temperature_high_);
    this->mark_alive_();
    return true;
  }
  return false;
}

void M5ChainImu::handle_event_(const ChainFrame &frame) {
  if (frame.data_size < 2 || frame.data[1] != IMU_EVENT_REPORT_TYPE) {
    ESP_LOGW(TAG, "Malformed Chain IMU event report");
    return;
  }
  const uint8_t status = frame.data[0];
  ESP_LOGD(TAG, "Chain IMU event report 0x%02X", status);
  if (this->motion_binary_sensor_ != nullptr)
    this->motion_binary_sensor_->publish_state(status & static_cast<uint8_t>(ImuEventMode::IMU_EVENT_MODE_ACCEL));
  if (this->rotation_binary_sensor_ != nullptr)
    this->rotation_binary_sensor_->publish_state(status & static_cast<uint8_t>(ImuEventMode::IMU_EVENT_MODE_GYRO));
  if (this->temperature_alarm_binary_sensor_ != nullptr) {
    const uint8_t field = status & EVENT_TEMPERATURE_MASK;
    this->temperature_alarm_binary_sensor_->publish_state(field == EVENT_TEMPERATURE_HIGH_ACTIVE ||
                                                          field == EVENT_TEMPERATURE_LOW_ACTIVE);
  }
  this->mark_alive_();
}

void M5ChainImu::mark_alive_() {
  this->timeout_warning_active_ = false;
  this->status_clear_warning();
}

void M5ChainImu::on_request_timeout(uint8_t index, uint8_t command) {
  // Settings and LED values are kept and resent on the next poll; a missed reading is simply dropped.
  const ImuJob job = this->active_job_;
  this->active_job_ = ImuJob::IMU_JOB_COUNT;
  if (job < ImuJob::IMU_JOB_READ_ACCEL)
    this->queue_job_(job);
  if (!this->timeout_warning_active_) {
    ESP_LOGW(TAG, "Timeout waiting for Chain IMU response (index=%u, command=0x%02X)", index, command);
    this->timeout_warning_active_ = true;
  }
  this->status_set_warning();
}

}  // namespace esphome::m5_chain_imu
