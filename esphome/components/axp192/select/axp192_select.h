#pragma once

#include "esphome/components/axp_base/axp_base.h"
#include "esphome/components/select/select.h"
#include "esphome/core/component.h"

namespace esphome::axp192 {

/// Drives one of the chip's enum-like settings.
///
/// The options come from the model table's value table, in its order, so the
/// option index is the entry index and nothing has to be parsed back from the
/// option text.
class AXP192Select : public select::Select, public Component, public Parented<axp_base::AxpComponent> {
 public:
  /// Name of the setting as it appears in the model table.
  void set_select_name(const char *name) { this->select_name_ = name; }

  /// Option to force during setup(); negative keeps whatever the chip holds.
  void set_initial_option_index(int8_t index) { this->initial_option_index_ = index; }

  /// Runs at HARDWARE so a setting that another boot step depends on is in place
  /// before the DATA components and `on_boot` triggers at that priority run. Still
  /// after the hub, which sits at IO.
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void setup() override;
  void dump_config() override;

 protected:
  void control(size_t index) override;

  /// Publish the option the chip is actually set to.
  void publish_current_option_();

  /// Read the setting's register code.
  bool read_current_code_(uint8_t &code);
  /// Map a register code to its option index; false when the code has no entry.
  bool index_for_code_(uint8_t code, uint8_t &index) const;

  const char *select_name_{nullptr};
  const axp_base::AxpSelect *select_{nullptr};
  int8_t initial_option_index_{-1};
};

}  // namespace esphome::axp192
