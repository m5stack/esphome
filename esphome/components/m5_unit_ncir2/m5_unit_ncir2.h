#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/i2c/i2c.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

namespace esphome::m5_unit_ncir2 {

// I2C register map, from M5Stack's "Unit NCIR2 I2C Protocol" and the M5Unit-NCIR2 library
// (https://github.com/m5stack/M5Unit-NCIR2). Multi-byte values are little-endian.
enum : uint8_t {
  REG_TEMPERATURE = 0x00,        // int16_t, 0.01 °C
  REG_EMISSIVITY = 0x10,         // uint16_t, emissivity * 65535
  REG_ALARM_TEMPERATURE = 0x20,  // 2x int16_t (low, high), 0.01 °C
  REG_ALARM_LED = 0x30,          // 2x R, G, B (low, high)
  REG_ALARM_BUZZER = 0x40,       // 2x uint16_t Hz, uint16_t interval ms, uint8_t duty (low, high)
  REG_BUZZER = 0x50,             // uint16_t Hz, uint8_t duty, uint8_t enable
  REG_LED = 0x60,                // R, G, B
  REG_BUTTON = 0x70,             // 0 while pressed, 1 when released
  REG_SAVE_SETTINGS = 0x80,      // write 1 to store the emissivity and alarm settings in the unit's flash
  REG_CHIP_TEMPERATURE = 0x90,   // int16_t, 0.01 °C
  REG_FIRMWARE_VERSION = 0xFE,
};

static constexpr uint8_t ALARM_COUNT = 2;

/// Settings a number entity controls. Alarm settings alternate low, high, so the alarm index is the value % 2.
enum class Setting : uint8_t {
  SETTING_LOW_ALARM_TEMPERATURE,
  SETTING_HIGH_ALARM_TEMPERATURE,
  SETTING_LOW_ALARM_FREQUENCY,
  SETTING_HIGH_ALARM_FREQUENCY,
  SETTING_LOW_ALARM_INTERVAL,
  SETTING_HIGH_ALARM_INTERVAL,
  SETTING_LOW_ALARM_DUTY,
  SETTING_HIGH_ALARM_DUTY,
  SETTING_BUZZER_FREQUENCY,
  SETTING_BUZZER_DUTY,
};
static constexpr size_t SETTING_COUNT = 10;

/// Hub for one M5Stack Unit NCIR2 (MLX90614 behind an STM32 that adds an RGB LED, a buzzer and a button).
class M5UnitNCIR2 : public i2c::I2CDevice, public PollingComponent {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  /// Emissivity as the unit stores it, a fraction of 65535.
  void set_emissivity(uint16_t emissivity) {
    this->emissivity_ = emissivity;
    this->has_emissivity_ = true;
  }
  /// Alarm settings by index, 0 for the low alarm and 1 for the high one. Settings left unset keep the unit's own.
  void set_alarm_temperature(uint8_t index, int16_t centi_celsius) {
    this->alarms_[index].temperature = centi_celsius;
    this->alarms_[index].has_temperature = true;
  }
  void set_alarm_color(uint8_t index, uint8_t red, uint8_t green, uint8_t blue) {
    this->alarms_[index].color[0] = red;
    this->alarms_[index].color[1] = green;
    this->alarms_[index].color[2] = blue;
    this->alarms_[index].has_color = true;
  }
  void set_alarm_buzzer(uint8_t index, uint16_t frequency, uint16_t interval_ms, uint8_t duty) {
    this->alarms_[index].buzzer_frequency = frequency;
    this->alarms_[index].buzzer_interval = interval_ms;
    this->alarms_[index].buzzer_duty = duty;
    this->alarms_[index].has_buzzer = true;
  }

#ifdef USE_SENSOR
  SUB_SENSOR(temperature)
  SUB_SENSOR(internal_temperature)
#endif
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(button)
#endif
#ifdef USE_NUMBER
  void set_setting_number(Setting setting, number::Number *number) {
    this->setting_numbers_[static_cast<size_t>(setting)] = number;
  }
#endif
#ifdef USE_SWITCH
  SUB_SWITCH(buzzer)
#endif

  bool write_led(uint8_t red, uint8_t green, uint8_t blue);
  /// Sound the buzzer at `frequency` Hz; a duty of 0 silences it.
  bool write_buzzer(uint16_t frequency, uint8_t duty);
  /// Change a setting from its number entity: temperatures in °C, frequencies in Hz, intervals in ms, duties in %.
  /// Alarm changes are stored in the unit's flash once no change has come for a few seconds.
  bool control_setting(Setting setting, float value);
  /// Sound the buzzer with the tone its numbers set, or silence it.
  bool control_buzzer(bool on) { return this->write_buzzer(this->tone_frequency_, on ? this->tone_duty_ : 0); }

 protected:
  struct Alarm {
    int16_t temperature{0};
    uint16_t buzzer_frequency{0};
    uint16_t buzzer_interval{0};
    uint8_t color[3]{};
    uint8_t buzzer_duty{0};
    bool has_temperature{false};
    bool has_color{false};
    bool has_buzzer{false};
  };

  // The unit's firmware holds the bus low after a repeated START, so a read is a write, a STOP, then a read
  bool read_register_(uint8_t reg, uint8_t *data, size_t len);
  bool read_temperature_(uint8_t reg, float *celsius);
  bool write_settings_();
  bool read_setting_(Setting setting, float *value);
  void save_settings_();

  static constexpr uint32_t SAVE_TIMEOUT_ID = 0;
  static constexpr uint32_t SAVE_DONE_TIMEOUT_ID = 1;

  Alarm alarms_[ALARM_COUNT];
#ifdef USE_NUMBER
  number::Number *setting_numbers_[SETTING_COUNT]{};
#endif
  uint32_t last_button_poll_{0};
  uint16_t emissivity_{0};
  uint16_t tone_frequency_{4000};
  uint8_t tone_duty_{128};
  uint8_t firmware_version_{0};
  bool has_emissivity_{false};
  bool buzzer_on_{false};
  // Set for a second after a save, which the vendor library also leaves the unit alone for
  bool saving_{false};
};

}  // namespace esphome::m5_unit_ncir2
