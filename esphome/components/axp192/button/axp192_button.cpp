#include "axp192_button.h"

#include "esphome/core/log.h"

namespace esphome::axp192 {

ESPHOME_LOG_TAG(TAG, "axp192.button");

void AXP192Button::press_action() {
  switch (this->operation_) {
    case ChipOperation::AXP192_CHIP_OPERATION_POWER_OFF:
      if (!this->parent_->power_off()) {
        ESP_LOGE(TAG, "Could not power the chip off");
      }
      return;
    case ChipOperation::AXP192_CHIP_OPERATION_RESET_GAUGE:
      if (!this->parent_->reset_gauge()) {
        ESP_LOGE(TAG, "Could not reset the coulomb counters");
      }
      return;
  }
}

void AXP192Button::dump_config() {
  LOG_BUTTON("  ", "AXP192 Button", this);
  ESP_LOGCONFIG(TAG, "  Operation: %s",
                this->operation_ == ChipOperation::AXP192_CHIP_OPERATION_POWER_OFF ? LOG_STR_LITERAL("power off")
                                                                                   : LOG_STR_LITERAL("reset gauge"));
}

}  // namespace esphome::axp192
