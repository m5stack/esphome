#pragma once

#include "esphome/components/axp_base/axp_base.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"

namespace esphome::axp2101 {

/// Switches one power rail on or off.
///
/// The voltage is left alone: this entity only decides whether the rail is
/// enabled, so whatever set point the chip or an output already holds is kept.
class AXP2101Switch : public switch_::Switch, public Component, public Parented<axp_base::AxpComponent> {
 public:
  /// Name of the rail as it appears in the model table.
  void set_rail_name(const char *name) { this->rail_name_ = name; }

  void setup() override;
  void write_state(bool state) override;
  void dump_config() override;

 protected:
  const char *rail_name_{nullptr};
  const axp_base::AxpRail *rail_{nullptr};
};

}  // namespace esphome::axp2101
