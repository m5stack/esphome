#pragma once

#include "esphome/components/axp_base/axp_base.h"
#include "esphome/core/gpio.h"

namespace esphome::axp192 {

/// X-Powers AXP192.
///
/// Everything that makes this chip a PMIC - rails, ADC channels, status bits,
/// interrupt sources and the charger settings - lives in the model table in
/// axp192.cpp and is reachable through the shared axp_base API. What this class
/// adds is the part that is neither a rail nor a measurement: GPIO0 to GPIO4,
/// which also serve as the LDOIO0 output, the two PWM outputs and four ADC
/// inputs. Those pins are driven here so the ESPHome pin platform can use them.
class AXP192 : public axp_base::AxpComponent {
 public:
  AXP192();

  /// GPIO expander interface, called through AXP192GPIOPin.
  ///
  /// Pins 0 to 4 are the chip's free GPIOs. GPIO5 exists but shares the N_RSTO
  /// pin, which is the reset line of the application processor on every board
  /// that wires it up, so it is deliberately left out.
  ///
  /// The outputs are NMOS open drain: there is no push-pull mode, so an output
  /// selects the open drain function and the level selects between pulling the
  /// pin low and releasing it.
  void pin_mode(uint8_t pin, gpio::Flags flags);
  bool digital_read(uint8_t pin);
  void digital_write(uint8_t pin, bool value);

  void dump_config() override;
};

/// One GPIO of the expander, as the ESPHome pin platform builds it.
///
/// The class carries no state of its own beyond the pin number, its inversion and
/// the mode it was configured with; every access goes to the hub, which owns the
/// I2C connection.
class AXP192GPIOPin final : public GPIOPin {
 public:
  void set_parent(AXP192 *parent) { this->parent_ = parent; }
  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { this->flags_ = flags; }

  gpio::Flags get_flags() const override { return this->flags_; }

  void setup() override;
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  size_t dump_summary(char *buffer, size_t len) const override;

 protected:
  AXP192 *parent_{nullptr};
  uint8_t pin_{0};
  bool inverted_{false};
  gpio::Flags flags_{};
};

}  // namespace esphome::axp192
