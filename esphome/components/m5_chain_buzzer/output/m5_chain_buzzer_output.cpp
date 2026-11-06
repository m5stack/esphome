#include "m5_chain_buzzer_output.h"

#include <algorithm>

#include "../m5_chain_buzzer.h"

namespace esphome::m5_chain_buzzer {

void M5ChainBuzzerOutput::write_state(float state) {
  if (this->parent_ == nullptr)
    return;
  if (state <= 0.0f) {
    this->parent_->set_tone_off();
    return;
  }
  // RTTTL calls update_frequency() immediately before this, so the held tone
  // always uses the note that is about to play. The module cannot be told how
  // long a level lasts, so the tone is held until the next write_state().
  this->parent_->set_tone(this->frequency_, static_cast<uint8_t>(std::clamp(state, 0.0f, 1.0f) * 100.0f));
}

void M5ChainBuzzerOutput::update_frequency(float frequency) {
  this->frequency_ = static_cast<uint16_t>(std::clamp(frequency, 100.0f, 10000.0f));
}

}  // namespace esphome::m5_chain_buzzer
