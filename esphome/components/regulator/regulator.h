#pragma once

#include <cstddef>
#include <cstdint>

#include "esphome/core/component.h"

namespace esphome::regulator {

class Regulator;

/// Registry of the configured regulators, addressed by their YAML ID.
///
/// The consumer layer that lets other components ask for a regulator by ID is
/// not part of phase 1; this registry is the piece of it that already exists,
/// so providers are reachable without adding a lookup API later.
class RegulatorCore {
 public:
  static void register_regulator(Regulator *regulator);
  /// Look a regulator up by its configuration ID. Returns nullptr when absent.
  static Regulator *find_regulator(const char *name);
  /// How many regulators have been registered.
  static size_t get_regulator_count();
};

/// A switched power resource, with optional voltage control.
///
/// The base class owns the policy shared by every provider: apply the startup
/// voltage before switching on, honour boot_on/always_on, wait the startup
/// delay, refuse to switch off an always-on regulator, and check requested
/// voltages against both the hardware window and the configured limits.
/// Platforms only implement the register or pin access.
class Regulator : public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

  /// Switch the regulator on.
  bool enable();
  /// Switch the regulator off. Always-on regulators refuse and stay enabled.
  bool disable();
  /// Whether the hardware currently has the regulator switched on.
  virtual bool is_enabled() = 0;

  /// Whether this regulator can change its output voltage at all.
  bool has_voltage_control() const { return this->has_voltage_control_; }
  /// Effective limits: the hardware window narrowed by min_voltage/max_voltage.
  bool get_voltage_limits(uint16_t &min_mv, uint16_t &max_mv) const;
  /// Set the output voltage in millivolts, checked against the limits.
  bool set_voltage(uint16_t millivolt);
  /// Read the output voltage in millivolts. False when it has no control.
  bool get_voltage(uint16_t &millivolt);

  void set_regulator_name(const char *name) { this->name_ = name; }
  const char *get_regulator_name() const { return this->name_; }

  void set_boot_on(bool boot_on) { this->boot_on_ = boot_on; }
  /// True when the regulator is switched on during setup().
  bool is_boot_on() const { return this->boot_on_ || this->always_on_; }
  void set_always_on(bool always_on) { this->always_on_ = always_on; }
  bool is_always_on() const { return this->always_on_; }

  void set_startup_voltage(uint16_t millivolt) {
    this->startup_voltage_ = millivolt;
    this->has_startup_voltage_ = true;
  }
  void set_startup_delay(uint32_t delay_ms) { this->startup_delay_ = delay_ms; }
  /// Wait after switching the regulator on before it is considered ready.
  uint32_t get_startup_delay() const { return this->startup_delay_; }
  /// Lower bound a consumer may request, checked on top of the hardware window.
  void set_min_voltage(uint16_t millivolt) { this->min_voltage_ = millivolt; }
  /// Upper bound a consumer may request, checked on top of the hardware window.
  void set_max_voltage(uint16_t millivolt) { this->max_voltage_ = millivolt; }

 protected:
  /// Platform hook, called from setup() before voltage or boot state is applied.
  virtual bool init_regulator_() { return true; }
  virtual bool enable_hw_() = 0;
  virtual bool disable_hw_() = 0;
  virtual bool write_voltage_(uint16_t millivolt) = 0;
  virtual bool read_voltage_(uint16_t &millivolt) = 0;

  /// Declare the voltage window the hardware can produce. A platform with
  /// voltage control has to call this from init_regulator_().
  void set_supported_voltage_range_(uint16_t min_mv, uint16_t max_mv) {
    this->supported_min_voltage_ = min_mv;
    this->supported_max_voltage_ = max_mv;
    this->has_voltage_control_ = true;
  }
  const char *name_for_log_() const { return this->name_ != nullptr ? this->name_ : "regulator"; }

  const char *name_{nullptr};
  bool boot_on_{false};
  bool always_on_{false};
  bool has_startup_voltage_{false};
  uint16_t startup_voltage_{0};
  uint32_t startup_delay_{0};
  bool has_voltage_control_{false};
  uint16_t supported_min_voltage_{0};
  uint16_t supported_max_voltage_{0};
  uint16_t min_voltage_{0};
  uint16_t max_voltage_{UINT16_MAX};
};

}  // namespace esphome::regulator
