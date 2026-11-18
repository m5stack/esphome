#pragma once

#include "esphome/components/axp_base/axp_base.h"
#include "esphome/components/regulator/regulator.h"
#include "esphome/core/component.h"

namespace esphome::axp192 {

/// One switched rail of an AXP192, exposed as a regulator.
///
/// The rail table in axp_base owns the register addresses and the voltage
/// windows; this class only adds the regulator policy on top of them.
class AXP192Regulator final : public regulator::Regulator, public Parented<axp_base::AxpComponent> {
 public:
  /// Name of the rail as it appears in the model table.
  void set_rail_name(const char *name) { this->rail_name_ = name; }

  void dump_config() override;
  bool is_enabled() override;

 protected:
  bool init_regulator_() override;
  bool enable_hw_() override;
  bool disable_hw_() override;
  bool write_voltage_(uint16_t millivolt) override;
  bool read_voltage_(uint16_t &millivolt) override;

  const char *rail_name_{nullptr};
  const axp_base::AxpRail *rail_{nullptr};
};

}  // namespace esphome::axp192
