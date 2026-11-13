#pragma once

#include "esphome/components/axp_base/axp_base.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"

namespace esphome::axp2101 {

/// Switches one of the chip's on/off settings, such as charging or the fuel gauge.
///
/// Unlike a rail switch this has no state of its own: unless a restore mode is
/// configured it leaves the chip alone at boot and only reports what the chip is
/// doing, so adding the entity cannot silently turn charging off.
class AXP2101FlagSwitch : public switch_::Switch, public Component, public Parented<axp_base::AxpComponent> {
 public:
  /// Name of the setting as it appears in the model table.
  void set_flag_name(const char *name) { this->flag_name_ = name; }

  void setup() override;
  void write_state(bool state) override;
  void dump_config() override;

 protected:
  const char *flag_name_{nullptr};
  const axp_base::AxpFlag *flag_{nullptr};
};

}  // namespace esphome::axp2101
