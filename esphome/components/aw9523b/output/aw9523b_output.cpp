#include "aw9523b_output.h"

#include <cmath>

#include "esphome/components/output/float_output.h"
#include "esphome/core/log.h"

namespace esphome::aw9523b {

ESPHOME_LOG_TAG(TAG, "aw9523b.output");

void AW9523BFloatOutput::setup() {
  if (!this->parent_->setup_led_mode(this->pin_)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
  }
}

void AW9523BFloatOutput::dump_config() {
  ESP_LOGCONFIG(TAG,
                "AW9523B float output:\n"
                "  Pin: %u",
                this->pin_);
  LOG_FLOAT_OUTPUT(this);
}

void AW9523BFloatOutput::write_state(float state) {
  // 0 - 255 brightness of the pin's LED current register
  this->parent_->write_led_current(this->pin_, static_cast<uint8_t>(roundf(state * 255.0f)));
}

}  // namespace esphome::aw9523b
