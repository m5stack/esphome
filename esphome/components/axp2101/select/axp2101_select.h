#pragma once

#include "esphome/components/axp_base/axp_base.h"
#include "esphome/components/select/select.h"
#include "esphome/core/component.h"

namespace esphome::axp2101 {

/// Drives one of the chip's enum-like settings.
///
/// The options come from the model table's value table, in its order, so the
/// option index is the entry index and nothing has to be parsed back from the
/// option text.
class AXP2101Select : public select::Select, public Component, public Parented<axp_base::AxpComponent> {
 public:
  /// Name of the setting as it appears in the model table.
  void set_select_name(const char *name) { this->select_name_ = name; }

  void setup() override;
  void dump_config() override;

 protected:
  void control(size_t index) override;

  /// Publish the option the chip is actually set to.
  void publish_current_option_();

  const char *select_name_{nullptr};
  const axp_base::AxpSelect *select_{nullptr};
};

}  // namespace esphome::axp2101
