#include "si5351_output.h"

#include "esphome/core/log.h"


namespace esphome::si5351 {

static const char *const TAG = "si5351.output";

void Si5351Output::write_state(float state) {
  if (this->parent_ == nullptr) {
    return;
  }
  this->parent_->set_clock_enabled(this->channel_, state > 0.0f);
}

bool Si5351Output::set_frequency(double frequency) {
  if (this->parent_ == nullptr) {
    return false;
  }
  return this->parent_->set_clock_frequency(this->channel_, frequency);
}

void Si5351Output::dump_config() {
  ESP_LOGCONFIG(TAG, "Si5351 output:");
  ESP_LOGCONFIG(TAG, "  Channel: CLK%u", this->channel_);
}

} // namespace esphome::si5351

