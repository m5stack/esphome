#include "m5stack_ioe1_switch.h"

#include "esphome/core/log.h"

namespace esphome::m5stack_ioe1 {

ESPHOME_LOG_TAG(TAG, "m5stack_ioe1.switch");

void M5StackIOE1AW8737ASwitch::setup() {
  this->parent_->setup_aw8737a(this->pin_, this->pulse_num_);

  const bool initial_state = this->get_initial_state_with_restore_mode().value_or(false);
  this->parent_->start_aw8737a(initial_state);
  this->publish_state(initial_state);
}

void M5StackIOE1AW8737ASwitch::dump_config() {
  ESP_LOGCONFIG(TAG,
                "M5STACK_IOE1 AW8737A Switch:\n"
                "  Pin: IO%u\n"
                "  Pulse number: %u",
                this->pin_, this->pulse_num_);
  LOG_SWITCH("  ", "AW8737A Switch", this);
}

void M5StackIOE1AW8737ASwitch::write_state(bool state) {
  this->parent_->start_aw8737a(state);
  this->publish_state(state);
}

}  // namespace esphome::m5stack_ioe1
