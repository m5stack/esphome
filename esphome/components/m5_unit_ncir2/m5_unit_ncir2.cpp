#include "m5_unit_ncir2.h"

#include <cmath>

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::m5_unit_ncir2 {

ESPHOME_LOG_TAG(TAG, "m5_unit_ncir2");

static constexpr uint32_t BUTTON_POLL_INTERVAL_MS = 50;
// Alarm changes are saved once the number entities have been still this long, sparing the unit's flash
static constexpr uint32_t SAVE_DELAY_MS = 5000;
static constexpr uint32_t SAVE_SETTLE_MS = 1000;

namespace {

struct AlarmField {
  uint8_t reg;
  uint8_t size;
};

// What an alarm setting is, from its place in Setting
enum AlarmKind : uint8_t {
  ALARM_KIND_TEMPERATURE = 0,
  ALARM_KIND_FREQUENCY = 1,
  ALARM_KIND_INTERVAL = 2,
  ALARM_KIND_DUTY = 3,
};

AlarmKind alarm_kind(Setting setting) { return static_cast<AlarmKind>(static_cast<uint8_t>(setting) / 2); }

// Register and length of an alarm setting, as the vendor library reads and writes it
AlarmField alarm_field(Setting setting) {
  const uint8_t buzzer = REG_ALARM_BUZZER + static_cast<uint8_t>(setting) % 2 * 5;
  switch (alarm_kind(setting)) {
    case ALARM_KIND_TEMPERATURE:
      return {static_cast<uint8_t>(REG_ALARM_TEMPERATURE + static_cast<uint8_t>(setting) % 2 * 2), 2};
    case ALARM_KIND_FREQUENCY:
      return {buzzer, 2};
    case ALARM_KIND_INTERVAL:
      return {static_cast<uint8_t>(buzzer + 2), 2};
    default:
      return {static_cast<uint8_t>(buzzer + 4), 1};
  }
}

uint8_t percent_to_duty(float percent) { return static_cast<uint8_t>(lroundf(clamp(percent, 0.0f, 100.0f) * 2.55f)); }

uint16_t to_raw(Setting setting, float value) {
  switch (alarm_kind(setting)) {
    case ALARM_KIND_TEMPERATURE:
      return static_cast<uint16_t>(static_cast<int16_t>(lroundf(value * 100.0f)));
    case ALARM_KIND_DUTY:
      return percent_to_duty(value);
    default:
      return static_cast<uint16_t>(lroundf(value));
  }
}

float from_raw(Setting setting, uint16_t raw) {
  switch (alarm_kind(setting)) {
    case ALARM_KIND_TEMPERATURE:
      return static_cast<int16_t>(raw) / 100.0f;
    case ALARM_KIND_DUTY:
      return roundf(raw / 2.55f);
    default:
      return raw;
  }
}

}  // namespace

void M5UnitNCIR2::setup() {
  if (!this->read_register_(REG_FIRMWARE_VERSION, &this->firmware_version_, 1)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  if (!this->write_settings_()) {
    this->mark_failed(LOG_STR("Writing the settings failed"));
    return;
  }
#ifdef USE_NUMBER
  for (size_t index = 0; index < SETTING_COUNT; index++) {
    number::Number *number = this->setting_numbers_[index];
    float value{NAN};
    if (number == nullptr)
      continue;
    if (!this->read_setting_(static_cast<Setting>(index), &value)) {
      this->mark_failed(LOG_STR("Reading the settings failed"));
      return;
    }
    number->publish_state(value);
  }
#endif
#ifdef USE_SWITCH
  if (this->buzzer_switch_ != nullptr) {
    uint8_t enabled{0};
    if (!this->read_register_(REG_BUZZER + 3, &enabled, 1)) {
      this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
      return;
    }
    this->buzzer_on_ = enabled != 0;
    this->buzzer_switch_->publish_state(this->buzzer_on_);
  }
#endif
#ifdef USE_BINARY_SENSOR
  if (this->button_binary_sensor_ != nullptr)
    return;
#endif
  this->disable_loop();
}

bool M5UnitNCIR2::write_settings_() {
  if (this->has_emissivity_) {
    uint8_t current[2]{};
    if (!this->read_register_(REG_EMISSIVITY, current, sizeof(current)))
      return false;
    // The unit restarts its MLX90614 for every emissivity write, so only write a change
    if (encode_uint16(current[1], current[0]) != this->emissivity_) {
      const uint8_t data[2] = {static_cast<uint8_t>(this->emissivity_), static_cast<uint8_t>(this->emissivity_ >> 8)};
      if (!this->write_bytes(REG_EMISSIVITY, data, sizeof(data)))
        return false;
    }
  }
  for (uint8_t index = 0; index < ALARM_COUNT; index++) {
    const Alarm &alarm = this->alarms_[index];
    if (alarm.has_temperature) {
      const uint8_t data[2] = {static_cast<uint8_t>(alarm.temperature),
                               static_cast<uint8_t>(static_cast<uint16_t>(alarm.temperature) >> 8)};
      if (!this->write_bytes(REG_ALARM_TEMPERATURE + index * 2, data, sizeof(data)))
        return false;
    }
    if (alarm.has_color && !this->write_bytes(REG_ALARM_LED + index * 3, alarm.color, sizeof(alarm.color)))
      return false;
    if (alarm.has_buzzer) {
      // Field by field, as the vendor library writes them
      const uint8_t reg = REG_ALARM_BUZZER + index * 5;
      const uint8_t frequency[2] = {static_cast<uint8_t>(alarm.buzzer_frequency),
                                    static_cast<uint8_t>(alarm.buzzer_frequency >> 8)};
      const uint8_t interval[2] = {static_cast<uint8_t>(alarm.buzzer_interval),
                                   static_cast<uint8_t>(alarm.buzzer_interval >> 8)};
      if (!this->write_bytes(reg, frequency, sizeof(frequency)) ||
          !this->write_bytes(reg + 2, interval, sizeof(interval)) || !this->write_byte(reg + 4, alarm.buzzer_duty))
        return false;
    }
  }
  return true;
}

void M5UnitNCIR2::loop() {
#ifdef USE_BINARY_SENSOR
  if (this->saving_)
    return;
  const uint32_t now = App.get_loop_component_start_time();
  if (now - this->last_button_poll_ < BUTTON_POLL_INTERVAL_MS)
    return;
  this->last_button_poll_ = now;
  uint8_t released{1};
  if (!this->status_comm_result(this->read_register_(REG_BUTTON, &released, 1)))
    return;
  this->button_binary_sensor_->publish_state(released == 0);
#endif
}

void M5UnitNCIR2::update() {
#ifdef USE_SENSOR
  if (this->saving_)
    return;
  float celsius{NAN};
  if (this->temperature_sensor_ != nullptr) {
    if (!this->status_comm_result(this->read_temperature_(REG_TEMPERATURE, &celsius)))
      return;
    this->temperature_sensor_->publish_state(celsius);
  }
  if (this->internal_temperature_sensor_ != nullptr) {
    if (!this->status_comm_result(this->read_temperature_(REG_CHIP_TEMPERATURE, &celsius)))
      return;
    this->internal_temperature_sensor_->publish_state(celsius);
  }
#endif
}

void M5UnitNCIR2::dump_config() {
  ESP_LOGCONFIG(TAG,
                "M5Stack Unit NCIR2:\n"
                "  Firmware version: %u",
                this->firmware_version_);
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  if (this->has_emissivity_) {
    ESP_LOGCONFIG(TAG, "  Emissivity: %.3f", this->emissivity_ / 65535.0f);
  }
  for (uint8_t index = 0; index < ALARM_COUNT; index++) {
    if (this->alarms_[index].has_temperature) {
      ESP_LOGCONFIG(TAG, "  %s alarm: %.2f °C", index == 0 ? LOG_STR_LITERAL("Low") : LOG_STR_LITERAL("High"),
                    this->alarms_[index].temperature / 100.0f);
    }
  }
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Internal temperature", this->internal_temperature_sensor_);
#endif
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Button", this->button_binary_sensor_);
#endif
}

bool M5UnitNCIR2::write_led(uint8_t red, uint8_t green, uint8_t blue) {
  if (this->is_failed() || this->saving_)
    return false;
  const uint8_t data[3] = {red, green, blue};
  return this->status_comm_result(this->write_bytes(REG_LED, data, sizeof(data)));
}

bool M5UnitNCIR2::write_buzzer(uint16_t frequency, uint8_t duty) {
  if (this->is_failed() || this->saving_)
    return false;
  const uint8_t data[4] = {static_cast<uint8_t>(frequency), static_cast<uint8_t>(frequency >> 8), duty,
                           static_cast<uint8_t>(duty != 0)};
  if (!this->status_comm_result(this->write_bytes(REG_BUZZER, data, sizeof(data))))
    return false;
  this->buzzer_on_ = duty != 0;
#ifdef USE_SWITCH
  // The output, rtttl for one, drives the buzzer too
  if (this->buzzer_switch_ != nullptr && this->buzzer_switch_->state != this->buzzer_on_)
    this->buzzer_switch_->publish_state(this->buzzer_on_);
#endif
  return true;
}

bool M5UnitNCIR2::control_setting(Setting setting, float value) {
  if (this->is_failed() || this->saving_)
    return false;
  if (setting == Setting::SETTING_BUZZER_FREQUENCY || setting == Setting::SETTING_BUZZER_DUTY) {
    if (setting == Setting::SETTING_BUZZER_FREQUENCY) {
      this->tone_frequency_ = static_cast<uint16_t>(lroundf(clamp(value, 20.0f, 20000.0f)));
    } else {
      this->tone_duty_ = percent_to_duty(value);
    }
    return !this->buzzer_on_ || this->write_buzzer(this->tone_frequency_, this->tone_duty_);
  }
  const AlarmField field = alarm_field(setting);
  const uint16_t raw = to_raw(setting, value);
  const uint8_t data[2] = {static_cast<uint8_t>(raw), static_cast<uint8_t>(raw >> 8)};
  if (!this->status_comm_result(this->write_bytes(field.reg, data, field.size)))
    return false;
  this->set_timeout(SAVE_TIMEOUT_ID, SAVE_DELAY_MS, [this]() { this->save_settings_(); });
  return true;
}

void M5UnitNCIR2::save_settings_() {
  if (this->is_failed() || !this->status_comm_result(this->write_byte(REG_SAVE_SETTINGS, 1)))
    return;
  this->saving_ = true;
  this->set_timeout(SAVE_DONE_TIMEOUT_ID, SAVE_SETTLE_MS, [this]() { this->saving_ = false; });
}

bool M5UnitNCIR2::read_setting_(Setting setting, float *value) {
  if (setting == Setting::SETTING_BUZZER_FREQUENCY) {
    *value = this->tone_frequency_;
    return true;
  }
  if (setting == Setting::SETTING_BUZZER_DUTY) {
    *value = roundf(this->tone_duty_ / 2.55f);
    return true;
  }
  const AlarmField field = alarm_field(setting);
  uint8_t data[2]{};
  if (!this->read_register_(field.reg, data, field.size))
    return false;
  *value = from_raw(setting, encode_uint16(data[1], data[0]));
  return true;
}

bool M5UnitNCIR2::read_register_(uint8_t reg, uint8_t *data, size_t len) {
  return this->write(&reg, 1) == i2c::ERROR_OK && this->read(data, len) == i2c::ERROR_OK;
}

bool M5UnitNCIR2::read_temperature_(uint8_t reg, float *celsius) {
  uint8_t data[2]{};
  if (!this->read_register_(reg, data, sizeof(data)))
    return false;
  *celsius = static_cast<int16_t>(encode_uint16(data[1], data[0])) / 100.0f;
  return true;
}

}  // namespace esphome::m5_unit_ncir2
