#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/i2c/i2c.h"
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

namespace esphome::m5_unit_acssr {

// I2C register map, from the M5Unit-ACSSR library and the unit's firmware
// (https://github.com/m5stack/M5Unit-ACSSR-Internal-FW). The Unit DCSSR runs the same firmware.
enum : uint8_t {
  REG_RELAY = 0x00,  // 0 off, 1 on
  REG_LED = 0x10,    // R, G, B
  REG_FIRMWARE_VERSION = 0xFE,
};

/// Hub for one M5Stack Unit ACSSR or DCSSR: a solid state relay and an RGB LED behind an ESP32 I2C target.
/// The unit forgets both when it restarts, so the hub holds them and writes them again.
class M5UnitACSSR : public i2c::I2CDevice, public PollingComponent {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

#ifdef USE_SWITCH
  SUB_SWITCH(relay)
#endif

  bool write_relay(bool on);
  bool write_led(uint8_t red, uint8_t green, uint8_t blue);

 protected:
  bool read_register_(uint8_t reg, uint8_t *data, size_t len);
  /// Check the unit answers, then bring it to the state the hub holds.
  bool connect_();
  void retry_connect_();
  bool write_state_();
  void lost_();

  static constexpr uint32_t CONNECT_INTERVAL_ID = 0;

  uint8_t led_[3]{};
  uint8_t firmware_version_{0};
  uint8_t connect_attempts_{0};
  bool relay_on_{false};
  // False until the relay state comes from the switch's restore mode or from the unit
  bool relay_known_{false};
  bool connected_{false};
  // True while setup()'s fast reconnect interval runs
  bool retrying_{false};
};

}  // namespace esphome::m5_unit_acssr
