#pragma once

#include "esphome/components/axp_base/axp_base.h"
#include "esphome/components/button/button.h"
#include "esphome/core/component.h"

namespace esphome::axp192 {

/// Which one-shot control a button runs.
enum class ChipOperation : uint8_t {
  AXP192_CHIP_OPERATION_POWER_OFF,
  AXP192_CHIP_OPERATION_RESET_GAUGE,
};

/// A button that runs one of the chip's one-shot controls.
///
/// The controls take the device down or restart its charge accounting, so they
/// are deliberately separate entities rather than something an automation does
/// implicitly.
class AXP192Button : public button::Button, public Component, public Parented<axp_base::AxpComponent> {
 public:
  void set_operation(ChipOperation operation) { this->operation_ = operation; }

  void dump_config() override;

 protected:
  void press_action() override;

  ChipOperation operation_{ChipOperation::AXP192_CHIP_OPERATION_POWER_OFF};
};

}  // namespace esphome::axp192
