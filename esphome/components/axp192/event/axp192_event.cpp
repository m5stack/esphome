#include "axp192_event.h"

#include "esphome/core/log.h"

namespace esphome::axp192 {

ESPHOME_LOG_TAG(TAG, "axp192.event");

void AXP192Event::setup() {
  if (this->parent_ == nullptr) {
    this->mark_failed(LOG_STR("No PMIC configured"));
    return;
  }

  for (const char *name : this->get_event_types()) {
    const int16_t index = this->parent_->find_event_index(name);
    if (index < 0) {
      ESP_LOGE(TAG, "'%s' is not an interrupt source of this chip", name);
      this->mark_failed(LOG_STR("Unknown interrupt source"));
      return;
    }
    this->event_mask_ |= uint64_t{1} << index;
  }
  if (this->event_mask_ == 0) {
    this->mark_failed(LOG_STR("No interrupt sources configured"));
    return;
  }

  if (!this->parent_->add_events_enabled(this->event_mask_)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  this->parent_->add_on_event([this](uint64_t mask) { this->on_events_(mask); });
}

void AXP192Event::on_events_(uint64_t mask) {
  const uint64_t fired = mask & this->event_mask_;
  if (fired == 0) {
    return;
  }

  const axp_base::AxpModel &model = this->parent_->get_model();
  for (uint8_t i = 0; i < model.event_count; i++) {
    if ((fired & (uint64_t{1} << i)) != 0) {
      this->trigger(model.events[i].name);
    }
  }
}

void AXP192Event::dump_config() {
  LOG_EVENT("  ", "AXP192 Event", this);
  ESP_LOGCONFIG(TAG, "  Interrupt sources: %u", static_cast<unsigned>(this->get_event_types().size()));
}

}  // namespace esphome::axp192
