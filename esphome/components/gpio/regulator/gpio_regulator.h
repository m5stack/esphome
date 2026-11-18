#pragma once

#include "esphome/components/regulator/regulator.h"
#include "esphome/core/gpio.h"

namespace esphome::gpio {

/// A load switch on a GPIO pin.
///
/// There is no voltage control: the pin only switches the supply on or off.
/// Without boot_on the pin is driven inactive during setup, so the load does
/// not depend on the level the pin happened to hold through reset.
class GPIORegulator final : public regulator::Regulator {
 public:
  void set_pin(GPIOPin *pin) { this->pin_ = pin; }

  void dump_config() override;
  float get_setup_priority() const override;
  bool is_enabled() override { return this->enabled_; }

 protected:
  bool init_regulator_() override;
  bool enable_hw_() override;
  bool disable_hw_() override;
  bool write_voltage_(uint16_t) override { return false; }
  bool read_voltage_(uint16_t &) override { return false; }

  GPIOPin *pin_{nullptr};
  bool enabled_{false};
};

}  // namespace esphome::gpio
