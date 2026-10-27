#include "m5_chain_switch_switch.h"

#include "esphome/core/log.h"

namespace esphome::m5_chain_switch {

ESPHOME_LOG_TAG(TAG, "m5_chain_switch.switch");

void M5ChainSwitchEventTriggerSwitch::write_state(bool state) {
  if (this->parent_ == nullptr)
    return;
  const auto status = this->parent_->set_event_trigger(state);
  if (status != m5_chain_base::ChainStatus::CHAIN_STATUS_OK) {
    ESP_LOGW(TAG, "Failed to set event trigger (status=0x%02X)", static_cast<uint8_t>(status));
  }
}

}  // namespace esphome::m5_chain_switch
