#include "m5_unit_8servos2_chain_switch.h"

#include "esphome/core/log.h"

namespace esphome::m5_unit_8servos2_chain {

ESPHOME_LOG_TAG(TAG, "m5_unit_8servos2_chain.switch");

void M5Unit8Servos2Switch::setup() {
  this->parent_->set_level_switch(this->channel_, this);
  this->parent_->request_mode(this->channel_, Servos2Mode::SERVOS2_MODE_OUTPUT);
  this->restore(false);
}

void M5Unit8Servos2Switch::dump_config() {
  ESP_LOGCONFIG(TAG, "M5Stack Unit 8Servos2 Chain Switch:");
  ESP_LOGCONFIG(TAG, "  Channel: %u", this->channel_);
  LOG_SWITCH(" ", "Switch", this);
}

void M5Unit8Servos2Switch::write_state(bool state) {
  if (this->parent_ != nullptr)
    this->parent_->set_output_level_desired(this->channel_, state);
}

}  // namespace esphome::m5_unit_8servos2_chain
