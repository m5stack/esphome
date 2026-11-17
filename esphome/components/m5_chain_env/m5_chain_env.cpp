#include "m5_chain_env.h"

#include <cinttypes>
#include <type_traits>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::m5_chain_env {

ESPHOME_LOG_TAG(TAG, "m5_chain_env");

using m5_chain_base::ChainCommand;
using m5_chain_base::ChainFrame;
using m5_chain_base::ChainStatus;

static constexpr uint8_t cmd(EnvCommand command) { return static_cast<uint8_t>(command); }
static constexpr uint8_t cmd(ChainCommand command) { return static_cast<uint8_t>(command); }
static constexpr uint16_t job_bit(EnvJob job) { return 1u << static_cast<uint8_t>(job); }

/// Second byte of a 0xE0 report: the device type code of the Chain ENV.
static constexpr uint8_t ENV_EVENT_REPORT_TYPE = 0x0F;
/// AHT20 answer: temperature (int16, C x100) and humidity (uint8, %).
static constexpr uint16_t AHT20_DATA_SIZE = 3;
/// SPA06 answer of the first protocol revision: temperature, pressure, altitude (int16, cm).
static constexpr uint16_t SPA06_SHORT_DATA_SIZE = 6;
/// SPA06 answer of current firmware: temperature, pressure, altitude (int32, cm).
static constexpr uint16_t SPA06_DATA_SIZE = 8;
static constexpr uint8_t OPERATION_SUCCESS = 1;

/// Give an alarm its first state from a reading; once it has one, only the device's reports change it.
static void publish_initial_alarm(binary_sensor::BinarySensor *alarm, bool active) {
  if (alarm != nullptr && !alarm->has_state())
    alarm->publish_state(active);
}

/// Write `value` least significant byte first, as every multi-byte field of the protocol is.
template<typename T> static void put_le(uint8_t *data, T value) {
  for (size_t i = 0; i < sizeof(T); i++)
    data[i] = static_cast<uint8_t>(static_cast<std::make_unsigned_t<T>>(value) >> (8 * i));
}

void M5ChainEnv::setup() {
  // Always send the event mode: it switches off the alarms that have no binary sensor, so the device
  // neither reports them nor tints its LED for them.
  this->queue_job_(EnvJob::ENV_JOB_EVENT_MODE);
  this->send_next_job_();
}

void M5ChainEnv::dump_config() {
  this->dump_base_config();
  ESP_LOGCONFIG(TAG,
                "M5Stack Chain ENV:\n"
                "  Chain index: %u",
                this->get_chain_index());
  if (this->sea_level_pressure_x10_ != 0) {
    ESP_LOGCONFIG(TAG, "  Sea level pressure: %.1f hPa", this->sea_level_pressure_x10_ / 10.0f);
  }
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
  LOG_SENSOR("  ", "Altitude", this->altitude_sensor_);
  LOG_SENSOR("  ", "Barometer temperature", this->barometer_temperature_sensor_);
  LOG_BINARY_SENSOR("  ", "Temperature alarm", this->temperature_alarm_binary_sensor_);
  if (this->temperature_alarm_binary_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "    Thresholds: %u to %u C (%s)", this->temperature_low_, this->temperature_high_,
                  this->temperature_from_spa06_ ? LOG_STR_LITERAL("SPA06") : LOG_STR_LITERAL("AHT20"));
  }
  LOG_BINARY_SENSOR("  ", "Humidity alarm", this->humidity_alarm_binary_sensor_);
  if (this->humidity_alarm_binary_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "    Thresholds: %u to %u %%", this->humidity_low_, this->humidity_high_);
  }
  LOG_BINARY_SENSOR("  ", "Pressure alarm", this->pressure_alarm_binary_sensor_);
  if (this->pressure_alarm_binary_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "    Thresholds: %.1f to %.1f hPa", this->pressure_low_x10_ / 10.0f,
                  this->pressure_high_x10_ / 10.0f);
  }
  LOG_BINARY_SENSOR("  ", "Altitude alarm", this->altitude_alarm_binary_sensor_);
  if (this->altitude_alarm_binary_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "    Thresholds: %.2f to %.2f m", this->altitude_low_cm_ / 100.0f,
                  this->altitude_high_cm_ / 100.0f);
  }
}

void M5ChainEnv::set_sea_level_pressure(uint16_t pressure_x10) {
  this->sea_level_pressure_x10_ = pressure_x10;
  this->queue_job_(EnvJob::ENV_JOB_SEA_LEVEL_PRESSURE);
}

void M5ChainEnv::set_temperature_alarm(uint8_t low, uint8_t high, bool use_spa06) {
  this->temperature_low_ = low;
  this->temperature_high_ = high;
  this->temperature_from_spa06_ = use_spa06;
  this->event_mode_ |= 1u << static_cast<uint8_t>(EnvAlarm::ENV_ALARM_TEMPERATURE);
  this->queue_job_(EnvJob::ENV_JOB_TEMPERATURE_THRESHOLD);
}

void M5ChainEnv::set_humidity_alarm(uint8_t low, uint8_t high) {
  this->humidity_low_ = low;
  this->humidity_high_ = high;
  this->event_mode_ |= 1u << static_cast<uint8_t>(EnvAlarm::ENV_ALARM_HUMIDITY);
  this->queue_job_(EnvJob::ENV_JOB_HUMIDITY_THRESHOLD);
}

void M5ChainEnv::set_pressure_alarm(uint16_t low_x10, uint16_t high_x10) {
  this->pressure_low_x10_ = low_x10;
  this->pressure_high_x10_ = high_x10;
  this->event_mode_ |= 1u << static_cast<uint8_t>(EnvAlarm::ENV_ALARM_PRESSURE);
  this->queue_job_(EnvJob::ENV_JOB_PRESSURE_THRESHOLD);
}

void M5ChainEnv::set_altitude_alarm(int32_t low_cm, int32_t high_cm) {
  this->altitude_low_cm_ = low_cm;
  this->altitude_high_cm_ = high_cm;
  this->event_mode_ |= 1u << static_cast<uint8_t>(EnvAlarm::ENV_ALARM_ALTITUDE);
  this->queue_job_(EnvJob::ENV_JOB_ALTITUDE_THRESHOLD);
}

ChainStatus M5ChainEnv::set_rgb_color(uint8_t red, uint8_t green, uint8_t blue) {
  this->rgb_red_ = red;
  this->rgb_green_ = green;
  this->rgb_blue_ = blue;
  this->queue_job_(EnvJob::ENV_JOB_RGB);
  this->send_next_job_();
  return ChainStatus::CHAIN_STATUS_OK;
}

ChainStatus M5ChainEnv::set_rgb_color_component(uint8_t component, uint8_t value) {
  if (component > 2)
    return ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  uint8_t *channels[] = {&this->rgb_red_, &this->rgb_green_, &this->rgb_blue_};
  *channels[component] = value;
  this->queue_job_(EnvJob::ENV_JOB_RGB);
  this->send_next_job_();
  return ChainStatus::CHAIN_STATUS_OK;
}

ChainStatus M5ChainEnv::set_led_brightness(uint8_t brightness) {
  if (brightness > 100)
    return ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  this->brightness_ = brightness;
  this->queue_job_(EnvJob::ENV_JOB_BRIGHTNESS);
  this->send_next_job_();
  return ChainStatus::CHAIN_STATUS_OK;
}

void M5ChainEnv::poll_next_() {
  if (this->temperature_sensor_ != nullptr || this->humidity_sensor_ != nullptr)
    this->queue_job_(EnvJob::ENV_JOB_READ_AHT20);
  if (this->pressure_sensor_ != nullptr || this->altitude_sensor_ != nullptr ||
      this->barometer_temperature_sensor_ != nullptr)
    this->queue_job_(EnvJob::ENV_JOB_READ_SPA06);
  this->send_next_job_();
}

void M5ChainEnv::queue_job_(EnvJob job) { this->pending_jobs_ |= job_bit(job); }

void M5ChainEnv::send_next_job_() {
  if (this->pending_jobs_ == 0 || this->request_pending())
    return;

  EnvJob job = EnvJob::ENV_JOB_COUNT;
  for (uint8_t i = 0; i < static_cast<uint8_t>(EnvJob::ENV_JOB_COUNT); i++) {
    if (this->pending_jobs_ & (1u << i)) {
      job = static_cast<EnvJob>(i);
      break;
    }
  }

  uint8_t data[8];
  uint16_t size = 0;
  uint8_t command = 0;
  switch (job) {
    case EnvJob::ENV_JOB_SEA_LEVEL_PRESSURE:
      command = cmd(EnvCommand::ENV_COMMAND_SET_SEA_LEVEL_PRESSURE);
      put_le<uint16_t>(data, this->sea_level_pressure_x10_);
      data[2] = 0;  // keep the device's flash untouched, the value is sent on every boot
      size = 3;
      break;
    case EnvJob::ENV_JOB_TEMPERATURE_THRESHOLD:
      command = cmd(EnvCommand::ENV_COMMAND_SET_TEMPERATURE_THRESHOLD);
      data[0] = this->temperature_from_spa06_;
      data[1] = this->temperature_low_;
      data[2] = this->temperature_high_;
      size = 3;
      break;
    case EnvJob::ENV_JOB_HUMIDITY_THRESHOLD:
      command = cmd(EnvCommand::ENV_COMMAND_SET_HUMIDITY_THRESHOLD);
      data[0] = this->humidity_low_;
      data[1] = this->humidity_high_;
      size = 2;
      break;
    case EnvJob::ENV_JOB_PRESSURE_THRESHOLD:
      command = cmd(EnvCommand::ENV_COMMAND_SET_PRESSURE_THRESHOLD);
      put_le<uint16_t>(data, this->pressure_low_x10_);
      put_le<uint16_t>(data + 2, this->pressure_high_x10_);
      size = 4;
      break;
    case EnvJob::ENV_JOB_ALTITUDE_THRESHOLD:
      command = cmd(EnvCommand::ENV_COMMAND_SET_ALTITUDE_THRESHOLD);
      put_le<int32_t>(data, this->altitude_low_cm_);
      put_le<int32_t>(data + 4, this->altitude_high_cm_);
      size = 8;
      break;
    case EnvJob::ENV_JOB_EVENT_MODE:
      command = cmd(EnvCommand::ENV_COMMAND_SET_EVENT_MODE);
      data[0] = this->event_mode_;
      size = 1;
      break;
    case EnvJob::ENV_JOB_RGB:
      command = cmd(ChainCommand::CHAIN_COMMAND_SET_RGB_VALUE);
      data[0] = 0;  // first LED
      data[1] = 1;  // one LED
      data[2] = this->rgb_red_;
      data[3] = this->rgb_green_;
      data[4] = this->rgb_blue_;
      size = 5;
      break;
    case EnvJob::ENV_JOB_BRIGHTNESS:
      command = cmd(ChainCommand::CHAIN_COMMAND_SET_RGB_LIGHT);
      data[0] = this->brightness_;
      data[1] = 0;  // do not save to flash
      size = 2;
      break;
    case EnvJob::ENV_JOB_READ_AHT20:
      command = cmd(EnvCommand::ENV_COMMAND_GET_AHT20_DATA);
      break;
    case EnvJob::ENV_JOB_READ_SPA06:
      command = cmd(EnvCommand::ENV_COMMAND_GET_SPA06_DATA);
      break;
    case EnvJob::ENV_JOB_COUNT:
      return;
  }

  const auto status = this->request(this->get_chain_index(), command, data, size);
  if (status == ChainStatus::CHAIN_STATUS_BUSY)
    return;
  // Clear the job when it leaves, not when it is answered: a new value queued while this one is on the
  // wire must be sent again afterwards.
  this->pending_jobs_ &= ~job_bit(job);
  if (status == ChainStatus::CHAIN_STATUS_OK) {
    this->active_job_ = job;
  } else {
    ESP_LOGW(TAG, "Could not queue command 0x%02X (status=%u)", command, static_cast<uint8_t>(status));
    this->status_set_warning();
  }
}

void M5ChainEnv::on_response(const ChainFrame &frame) {
  this->active_job_ = EnvJob::ENV_JOB_COUNT;
  if (!this->handle_measurement_(frame)) {
    // Every other request is a setting: its answer is one operation status byte.
    if (frame.data_size < 1 || frame.data[0] != OPERATION_SUCCESS) {
      ESP_LOGW(TAG, "Chain ENV rejected command 0x%02X", frame.command);
      this->status_set_warning();
    } else {
      this->mark_alive_();
    }
  }
  this->send_next_job_();
}

void M5ChainEnv::on_unsolicited(const ChainFrame &frame) {
  if (frame.index != this->get_chain_index())
    return;
  if (frame.command == cmd(EnvCommand::ENV_COMMAND_EVENT_REPORT)) {
    this->handle_event_(frame);
    return;
  }
  // A reading can arrive after the request timed out when it crosses several chain nodes.
  this->handle_measurement_(frame);
}

bool M5ChainEnv::handle_measurement_(const ChainFrame &frame) {
  if (frame.command == cmd(EnvCommand::ENV_COMMAND_GET_AHT20_DATA)) {
    if (frame.data_size < AHT20_DATA_SIZE) {
      this->status_set_warning();
      return true;
    }
    const float temperature = encode_value_le<int16_t>(frame.data) / 100.0f;
    const uint8_t humidity = frame.data[2];
    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(temperature);
    if (this->humidity_sensor_ != nullptr)
      this->humidity_sensor_->publish_state(humidity);
    if (!this->temperature_from_spa06_) {
      publish_initial_alarm(this->temperature_alarm_binary_sensor_,
                            temperature < this->temperature_low_ || temperature > this->temperature_high_);
    }
    publish_initial_alarm(this->humidity_alarm_binary_sensor_,
                          humidity < this->humidity_low_ || humidity > this->humidity_high_);
    this->mark_alive_();
    return true;
  }

  if (frame.command == cmd(EnvCommand::ENV_COMMAND_GET_SPA06_DATA)) {
    if (frame.data_size < SPA06_SHORT_DATA_SIZE) {
      this->status_set_warning();
      return true;
    }
    const float temperature = encode_value_le<int16_t>(frame.data) / 100.0f;
    const uint16_t pressure_x10 = encode_value_le<uint16_t>(frame.data + 2);
    const int32_t altitude_cm = frame.data_size >= SPA06_DATA_SIZE ? encode_value_le<int32_t>(frame.data + 4)
                                                                   : encode_value_le<int16_t>(frame.data + 4);
    if (this->barometer_temperature_sensor_ != nullptr)
      this->barometer_temperature_sensor_->publish_state(temperature);
    if (this->pressure_sensor_ != nullptr)
      this->pressure_sensor_->publish_state(pressure_x10 / 10.0f);
    if (this->altitude_sensor_ != nullptr)
      this->altitude_sensor_->publish_state(altitude_cm / 100.0f);
    if (this->temperature_from_spa06_) {
      publish_initial_alarm(this->temperature_alarm_binary_sensor_,
                            temperature < this->temperature_low_ || temperature > this->temperature_high_);
    }
    publish_initial_alarm(this->pressure_alarm_binary_sensor_,
                          pressure_x10 < this->pressure_low_x10_ || pressure_x10 > this->pressure_high_x10_);
    publish_initial_alarm(this->altitude_alarm_binary_sensor_,
                          altitude_cm < this->altitude_low_cm_ || altitude_cm > this->altitude_high_cm_);
    this->mark_alive_();
    return true;
  }
  return false;
}

void M5ChainEnv::handle_event_(const ChainFrame &frame) {
  if (frame.data_size < 2 || frame.data[1] != ENV_EVENT_REPORT_TYPE) {
    ESP_LOGW(TAG, "Malformed Chain ENV event report");
    return;
  }
  const uint8_t status = frame.data[0];
  ESP_LOGD(TAG, "Chain ENV alarm report 0x%02X", status);
  binary_sensor::BinarySensor *alarms[] = {
      this->temperature_alarm_binary_sensor_,
      this->humidity_alarm_binary_sensor_,
      this->pressure_alarm_binary_sensor_,
      this->altitude_alarm_binary_sensor_,
  };
  for (uint8_t i = 0; i < 4; i++) {
    if (alarms[i] == nullptr)
      continue;
    // 1: rose above the high threshold, 2: fell below the low threshold. 0 and 3 are the matching releases.
    const uint8_t field = (status >> (i * 2)) & 0x03;
    alarms[i]->publish_state(field == 1 || field == 2);
  }
  this->mark_alive_();
}

void M5ChainEnv::mark_alive_() {
  this->timeout_warning_active_ = false;
  this->status_clear_warning();
}

void M5ChainEnv::on_request_timeout(uint8_t index, uint8_t command) {
  // Settings and LED values are kept and resent on the next poll; a missed reading is simply dropped.
  const EnvJob job = this->active_job_;
  this->active_job_ = EnvJob::ENV_JOB_COUNT;
  if (job < EnvJob::ENV_JOB_READ_AHT20)
    this->queue_job_(job);
  if (!this->timeout_warning_active_) {
    ESP_LOGW(TAG, "Timeout waiting for Chain ENV response (index=%u, command=0x%02X)", index, command);
    this->timeout_warning_active_ = true;
  }
  this->status_set_warning();
}

}  // namespace esphome::m5_chain_env
