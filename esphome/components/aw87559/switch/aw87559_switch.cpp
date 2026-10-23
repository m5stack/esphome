/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "aw87559_switch.h"
#include "esphome/core/log.h"

namespace esphome::aw87559 {

static const char *const TAG = "switch.aw87559";

void AW87559Switch::setup() {
  // Keep every switch in sync with the amplifier, no matter which one changed it.
  this->parent_->add_on_pa_state_callback([this](bool state) { this->publish_state(state); });

  bool initial_state = this->get_initial_state_with_restore_mode().value_or(false);
  if (initial_state) {
    this->turn_on();
  } else {
    this->turn_off();
  }
}

void AW87559Switch::dump_config() { LOG_SWITCH("", "AW87559 PA Switch", this); }

void AW87559Switch::write_state(bool state) {
  if (this->parent_ == nullptr) {
    this->publish_state(state);
    return;
  }
  if (this->parent_->set_pa_enabled(state)) {
    this->publish_state(state);
  }
}

}  // namespace esphome::aw87559
