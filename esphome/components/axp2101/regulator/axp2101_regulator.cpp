#include "axp2101_regulator.h"

#include <algorithm>
#include <cstdint>

#include "esphome/core/log.h"

namespace esphome::axp2101 {

ESPHOME_LOG_TAG(TAG, "axp2101.regulator");

bool AXP2101Regulator::init_regulator_() {
  if (this->rail_name_ == nullptr) {
    this->mark_failed(LOG_STR("No rail configured"));
    return false;
  }
  if (this->parent_ == nullptr) {
    this->mark_failed(LOG_STR("No PMIC configured"));
    return false;
  }
  this->rail_ = this->parent_->find_rail(this->rail_name_);
  if (this->rail_ == nullptr) {
    ESP_LOGE(TAG, "'%s' is not a power rail of this chip", this->rail_name_);
    this->mark_failed(LOG_STR("Unknown rail"));
    return false;
  }

  if ((this->rail_->flags & axp_base::AXP_RAIL_ALWAYS_ON) != 0) {
    // The chip refuses to switch this rail off, so the regulator does too.
    this->set_always_on(true);
  }
  if (axp_base::axp_rail_has_voltage(*this->rail_)) {
    uint16_t min_mv = UINT16_MAX;
    uint16_t max_mv = 0;
    for (uint8_t i = 0; i < this->rail_->range_count; i++) {
      const axp_base::AxpVoltageRange &range = this->rail_->ranges[i];
      min_mv = std::min(min_mv, range.min_mv);
      const uint16_t top = range.min_mv + range.step_mv * (range.max_code - range.min_code);
      max_mv = std::max(max_mv, top);
    }
    this->set_supported_voltage_range_(min_mv, max_mv);
  }
  return true;
}

bool AXP2101Regulator::enable_hw_() {
  if (this->rail_ == nullptr || this->parent_ == nullptr)
    return false;
  if (!axp_base::axp_rail_has_enable(*this->rail_))
    return true;  // fixed output, already on
  return this->parent_->set_rail_enabled(this->rail_, true);
}

bool AXP2101Regulator::disable_hw_() {
  if (this->rail_ == nullptr || this->parent_ == nullptr)
    return false;
  if (!axp_base::axp_rail_has_enable(*this->rail_))
    return false;  // fixed output, it cannot be switched off
  return this->parent_->set_rail_enabled(this->rail_, false);
}

bool AXP2101Regulator::is_enabled() {
  if (this->rail_ == nullptr || this->parent_ == nullptr)
    return false;
  if (!axp_base::axp_rail_has_enable(*this->rail_))
    return true;
  return this->parent_->is_rail_enabled(this->rail_);
}

bool AXP2101Regulator::write_voltage_(uint16_t millivolt) {
  if (this->rail_ == nullptr || this->parent_ == nullptr)
    return false;
  return this->parent_->set_rail_voltage(this->rail_, millivolt);
}

bool AXP2101Regulator::read_voltage_(uint16_t &millivolt) {
  if (this->rail_ == nullptr || this->parent_ == nullptr)
    return false;
  return this->parent_->get_rail_voltage(this->rail_, millivolt);
}

void AXP2101Regulator::dump_config() {
  regulator::Regulator::dump_config();
  if (this->rail_ == nullptr)
    return;
  ESP_LOGCONFIG(TAG, "  Rail: %s (up to %u mA)", this->rail_->name, static_cast<unsigned>(this->rail_->max_ma));
}

}  // namespace esphome::axp2101
