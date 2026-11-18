#include "regulator.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::regulator {

ESPHOME_LOG_TAG(TAG, "regulator");

namespace {

#ifdef REGULATOR_COUNT
StaticVector<Regulator *, REGULATOR_COUNT> regulators;
#endif

}  // namespace

void RegulatorCore::register_regulator(Regulator *regulator) {
#ifdef REGULATOR_COUNT
  if (regulator == nullptr)
    return;
  for (Regulator *existing : regulators) {
    if (existing == regulator)
      return;
  }
  regulators.push_back(regulator);
#else
  (void) regulator;
#endif
}

Regulator *RegulatorCore::find_regulator(const char *name) {
  if (name == nullptr)
    return nullptr;
#ifdef REGULATOR_COUNT
  for (Regulator *regulator : regulators) {
    if (regulator != nullptr && regulator->get_regulator_name() != nullptr &&
        strcmp(regulator->get_regulator_name(), name) == 0) {
      return regulator;
    }
  }
#endif
  return nullptr;
}

size_t RegulatorCore::get_regulator_count() {
#ifdef REGULATOR_COUNT
  return regulators.size();
#else
  return 0;
#endif
}

void Regulator::setup() {
  if (!this->init_regulator_())
    return;

  if (this->has_voltage_control_) {
    uint16_t min_mv = 0;
    uint16_t max_mv = 0;
    if (!this->get_voltage_limits(min_mv, max_mv)) {
      this->mark_failed(LOG_STR("Configured voltage range is outside the hardware window"));
      return;
    }
  }

  if (this->has_startup_voltage_ && !this->set_voltage(this->startup_voltage_)) {
    this->mark_failed(LOG_STR("Startup voltage is not supported"));
    return;
  }

  if (!this->is_boot_on())
    return;

  if (!this->enable()) {
    this->mark_failed(LOG_STR("Could not enable at boot"));
    return;
  }
}

void Regulator::dump_config() {
  ESP_LOGCONFIG(TAG, "Regulator '%s':", this->name_for_log_());
  if (this->has_voltage_control_) {
    uint16_t min_mv = 0;
    uint16_t max_mv = 0;
    if (this->get_voltage_limits(min_mv, max_mv)) {
      ESP_LOGCONFIG(TAG, "  Voltage range: %u..%u mV", static_cast<unsigned>(min_mv), static_cast<unsigned>(max_mv));
    }
  } else {
    ESP_LOGCONFIG(TAG, "  Voltage: fixed, no control");
  }
  if (this->has_startup_voltage_) {
    ESP_LOGCONFIG(TAG, "  Startup voltage: %u mV", static_cast<unsigned>(this->startup_voltage_));
  }
  ESP_LOGCONFIG(TAG, "  Boot on: %s", YESNO(this->is_boot_on()));
  ESP_LOGCONFIG(TAG, "  Always on: %s", YESNO(this->always_on_));
  if (this->startup_delay_ > 0) {
    ESP_LOGCONFIG(TAG, "  Startup delay: %" PRIu32 " ms", this->startup_delay_);
  }
}

float Regulator::get_setup_priority() const {
  // After the buses and I/O expanders, so a PMIC hub or an expander pin is
  // ready, and before DATA so sensors find their supply already on.
  return setup_priority::HARDWARE;
}

bool Regulator::enable() {
  if (this->is_failed())
    return false;
  if (!this->enable_hw_())
    return false;
  // Give the output time to come up before anything starts using it.
  if (this->startup_delay_ > 0)
    delay(this->startup_delay_);
  return true;
}

bool Regulator::disable() {
  if (this->always_on_) {
    ESP_LOGW(TAG, "'%s' is marked always on and cannot be switched off", this->name_for_log_());
    return false;
  }
  if (this->is_failed())
    return false;
  return this->disable_hw_();
}

bool Regulator::get_voltage_limits(uint16_t &min_mv, uint16_t &max_mv) const {
  if (!this->has_voltage_control_)
    return false;
  min_mv = std::max(this->supported_min_voltage_, this->min_voltage_);
  max_mv = std::min(this->supported_max_voltage_, this->max_voltage_);
  return min_mv <= max_mv;
}

bool Regulator::set_voltage(uint16_t millivolt) {
  if (this->is_failed())
    return false;
  if (!this->has_voltage_control_) {
    ESP_LOGE(TAG, "'%s' has no voltage control", this->name_for_log_());
    return false;
  }
  if (millivolt < this->supported_min_voltage_ || millivolt > this->supported_max_voltage_) {
    ESP_LOGE(TAG, "'%s' cannot produce %u mV (hardware range is %u..%u mV)", this->name_for_log_(),
             static_cast<unsigned>(millivolt), static_cast<unsigned>(this->supported_min_voltage_),
             static_cast<unsigned>(this->supported_max_voltage_));
    return false;
  }
  if (millivolt < this->min_voltage_ || millivolt > this->max_voltage_) {
    ESP_LOGE(TAG, "'%s': %u mV is outside the configured %u..%u mV range", this->name_for_log_(),
             static_cast<unsigned>(millivolt), static_cast<unsigned>(this->min_voltage_),
             static_cast<unsigned>(this->max_voltage_));
    return false;
  }
  if (!this->write_voltage_(millivolt)) {
    ESP_LOGE(TAG, "'%s' could not be set to %u mV", this->name_for_log_(), static_cast<unsigned>(millivolt));
    return false;
  }
  return true;
}

bool Regulator::get_voltage(uint16_t &millivolt) {
  if (!this->has_voltage_control_)
    return false;
  return this->read_voltage_(millivolt);
}

}  // namespace esphome::regulator
