#pragma once

#include <array>
#include <cstdint>

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome::si12t {

/// Touch channels of the Si12T
static constexpr uint8_t CHANNEL_COUNT = 12;

#ifdef USE_TEXT_SENSOR
class SI12TTextSensor;
#endif

class SI12TComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void loop() override;
  void dump_config() override;

  /// CFIG register: MS, FTC, ILC and RTC, taken from the configuration
  void set_config_register(uint8_t value) { this->config_register_ = value; }
  /// CTRL register without SRST: SLEEP and the two reserved bits
  void set_control_register(uint8_t value) { this->control_register_ = value; }
  /// Reset the device during setup, so that it starts from its documented reset values
  void set_software_reset(bool reset) { this->software_reset_ = reset; }
  /// Interrupt output pin; when set, it replaces polling
  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
#ifdef USE_TEXT_SENSOR
  /// Attach the text sensor that reports `channel` (zero based); the hub publishes its levels
  void set_channel_sensor(uint8_t channel, SI12TTextSensor *sensor) { this->sensors_[channel] = sensor; }
#endif

 protected:
  /// Publish the intensity level of every channel from Output1..3, read in one transaction
  bool read_outputs_();
  /// Reset and configure the device, then enable the channels that have a text sensor
  bool configure_();

  static void IRAM_ATTR interrupt_handler(SI12TComponent *arg);

  InternalGPIOPin *interrupt_pin_{nullptr};
  uint32_t last_read_{0};
#ifdef USE_TEXT_SENSOR
  std::array<SI12TTextSensor *, CHANNEL_COUNT> sensors_{};  // one text sensor per channel
#endif
  uint8_t config_register_{0x22};
  uint8_t control_register_{0x07};
  bool software_reset_{true};
  volatile bool pending_{false};
};

}  // namespace esphome::si12t
