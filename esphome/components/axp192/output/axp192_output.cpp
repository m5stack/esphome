#include "axp192_output.h"

#include "esphome/core/log.h"

namespace esphome::axp192 {

ESPHOME_LOG_TAG(TAG, "axp192.output");

/// Below this many reachable voltages a dimmer looks like a handful of jumps
/// rather than a fade, so it is worth pointing out at startup.
static constexpr uint16_t AXP_OUTPUT_MIN_USEFUL_LEVELS = 4;

void AXP192FloatOutput::setup() {
  if (this->rail_name_ == nullptr) {
    this->mark_failed(LOG_STR("No rail configured"));
    return;
  }
  this->rail_ = this->parent_->find_rail(this->rail_name_);
  if (this->rail_ == nullptr) {
    ESP_LOGE(TAG, "'%s' is not a power rail of this chip", this->rail_name_);
    this->mark_failed(LOG_STR("Unknown rail"));
    return;
  }
  if (!axp_base::axp_rail_has_voltage(*this->rail_)) {
    ESP_LOGE(TAG, "%s has no voltage control", this->rail_->name);
    this->mark_failed(LOG_STR("Rail has no voltage control"));
    return;
  }
  if (this->min_voltage_ >= this->max_voltage_) {
    this->mark_failed(LOG_STR("min_voltage must be below max_voltage"));
    return;
  }

  const uint16_t levels =
      axp_base::AxpComponent::count_rail_levels(*this->rail_, this->min_voltage_, this->max_voltage_);
  if (levels < 2) {
    ESP_LOGE(TAG, "%s cannot produce enough voltages between %u and %u mV", this->rail_->name,
             static_cast<unsigned>(this->min_voltage_), static_cast<unsigned>(this->max_voltage_));
    this->mark_failed(LOG_STR("Range holds fewer than two voltages"));
    return;
  }
  if (levels < AXP_OUTPUT_MIN_USEFUL_LEVELS) {
    ESP_LOGW(TAG, "%s gives only %u levels between %u and %u mV; widen the range or pick a rail with a finer step",
             this->rail_->name, static_cast<unsigned>(levels), static_cast<unsigned>(this->min_voltage_),
             static_cast<unsigned>(this->max_voltage_));
  }
}

void AXP192FloatOutput::write_state(float state) {
  if (this->rail_ == nullptr) {
    return;
  }

  if (state <= 0.0f) {
    // A rail at its lowest set point is still lit, so 0 means off.
    if (!this->parent_->set_rail_enabled(this->rail_, false)) {
      ESP_LOGE(TAG, "Could not turn %s off", this->rail_->name);
    }
    return;
  }

  const uint32_t span = static_cast<uint32_t>(this->max_voltage_ - this->min_voltage_);
  const uint16_t target = static_cast<uint16_t>(this->min_voltage_ + static_cast<uint32_t>(state * span + 0.5f));

  uint16_t applied = 0;
  if (!this->parent_->set_rail_voltage_nearest(this->rail_, target, &applied)) {
    ESP_LOGE(TAG, "Could not set %s to %u mV", this->rail_->name, static_cast<unsigned>(target));
    return;
  }
  // The set point goes in before the rail is enabled so it never comes up at
  // whatever voltage the chip happened to be holding.
  if (!this->parent_->set_rail_enabled(this->rail_, true)) {
    ESP_LOGE(TAG, "Could not turn %s on at %u mV", this->rail_->name, static_cast<unsigned>(applied));
  }

  ESP_LOGV(TAG, "%s: level %.2f -> %u mV (rounded from %u)", this->rail_->name, state, static_cast<unsigned>(applied),
           static_cast<unsigned>(target));
}

void AXP192FloatOutput::dump_config() {
  LOG_FLOAT_OUTPUT(this);
  if (this->rail_ == nullptr) {
    return;
  }
  ESP_LOGCONFIG(TAG, "  Rail: %s (%u mA max)", this->rail_->name, static_cast<unsigned>(this->rail_->max_ma));
  ESP_LOGCONFIG(TAG, "  Range: %u..%u mV, %u levels", static_cast<unsigned>(this->min_voltage_),
                static_cast<unsigned>(this->max_voltage_),
                static_cast<unsigned>(
                    axp_base::AxpComponent::count_rail_levels(*this->rail_, this->min_voltage_, this->max_voltage_)));
}

}  // namespace esphome::axp192
