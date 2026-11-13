#include "axp_base.h"

#include <cmath>
#include <cstring>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::axp_base {

ESPHOME_LOG_TAG(TAG, "axp_base");

/// How often loop() re-reads the interrupt registers for events that arrived
/// while the previous batch was being handled.
static constexpr uint8_t AXP_IRQ_DRAIN_ROUNDS = 4;

static uint16_t absolute_difference(uint16_t lhs, uint16_t rhs) { return lhs > rhs ? lhs - rhs : rhs - lhs; }

void AxpComponent::setup() {
  ESP_LOGD(TAG, "Setting up %s...", this->model_->name);

  uint8_t chip_id = 0;
  if (!this->read_reg_(this->model_->chip_id_reg, chip_id)) {
    this->mark_failed(LOG_STR("Failed to read the chip ID"));
    return;
  }
  if (chip_id != this->model_->chip_id) {
    ESP_LOGE(TAG, "Expected chip ID 0x%02X but read 0x%02X", this->model_->chip_id, chip_id);
    this->mark_failed(LOG_STR("Unexpected chip ID"));
    return;
  }
  this->chip_ready_ = true;

  if (!this->init_chip_()) {
    this->mark_failed(LOG_STR("Initialisation failed"));
    return;
  }

  if (this->irq_pin_ != nullptr) {
    this->irq_pin_->setup();
    // The interrupt line is active low on every AXP chip.
    this->irq_pin_->attach_interrupt(irq_isr_, this, gpio::INTERRUPT_FALLING_EDGE);
  }

  this->update_status();
}

void AxpComponent::loop() {
  if (!this->should_drain_events_()) {
    return;
  }

  // One interrupt edge can stand for several pending events, and clearing the
  // status registers releases the IRQ line so the next event raises a fresh
  // edge. Draining covers the events that arrived together.
  for (uint8_t round = 0; round < AXP_IRQ_DRAIN_ROUNDS; round++) {
    this->irq_pending_ = false;
    uint64_t mask = 0;
    if (!this->read_and_clear_events(mask) || mask == 0) {
      return;
    }
    this->event_callback_.call(mask);
  }
}

bool AxpComponent::should_drain_events_() {
  // Nobody is listening, so the status registers are left alone.
  if (this->events_enabled_mask_ == 0) {
    return false;
  }
  if (this->irq_pin_ != nullptr) {
    // The interrupt already told us there is something to read.
    return this->irq_pending_;
  }
  if (this->event_poll_interval_ == 0) {
    return false;
  }
  const uint32_t now = App.get_loop_component_start_time();
  if (now - this->last_event_poll_ < this->event_poll_interval_) {
    return false;
  }
  this->last_event_poll_ = now;
  return true;
}

void AxpComponent::update() {
  this->update_status();
#ifdef AXP_POLLED_ENTITY_COUNT
  // Publish everything from one poll so the entities share the hub's reads
  // instead of each opening its own bus transactions.
  for (AxpPolledEntity *entity : this->polled_entities_) {
    entity->publish_from_chip();
  }
#endif
}

void AxpComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "%s:", this->model_->name);
  LOG_I2C_DEVICE(this);
  if (this->irq_pin_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  IRQ pin: wired");
  } else if (this->events_enabled_mask_ != 0) {
    ESP_LOGCONFIG(TAG, "  Events: polling every %u ms", static_cast<unsigned>(this->event_poll_interval_));
  }
  for (uint8_t i = 0; i < this->model_->rail_count; i++) {
    const AxpRail &rail = this->model_->rails[i];
    if (!axp_rail_has_enable(rail)) {
      ESP_LOGCONFIG(TAG, "  %s: always on", rail.name);
      continue;
    }
    const bool enabled = this->is_rail_enabled(&rail);
    uint16_t millivolt = 0;
    if (axp_rail_has_voltage(rail) && this->get_rail_voltage(&rail, millivolt)) {
      ESP_LOGCONFIG(TAG, "  %s: %s, %u mV", rail.name, enabled ? LOG_STR_LITERAL("on") : LOG_STR_LITERAL("off"),
                    static_cast<unsigned>(millivolt));
    } else {
      ESP_LOGCONFIG(TAG, "  %s: %s", rail.name, enabled ? LOG_STR_LITERAL("on") : LOG_STR_LITERAL("off"));
    }
  }
}

const AxpRail *AxpComponent::find_rail(const char *name) const {
  if (name == nullptr) {
    return nullptr;
  }
  for (uint8_t i = 0; i < this->model_->rail_count; i++) {
    if (strcmp(this->model_->rails[i].name, name) == 0) {
      return &this->model_->rails[i];
    }
  }
  return nullptr;
}

const AxpStatusFlag *AxpComponent::find_status_flag(const char *name) const {
  if (name == nullptr) {
    return nullptr;
  }
  for (uint8_t i = 0; i < this->model_->status_flag_count; i++) {
    if (strcmp(this->model_->status_flags[i].name, name) == 0) {
      return &this->model_->status_flags[i];
    }
  }
  return nullptr;
}

int16_t AxpComponent::find_event_index(const char *name) const {
  if (name == nullptr) {
    return -1;
  }
  const uint8_t count = this->model_->event_count < AXP_MAX_EVENTS ? this->model_->event_count : AXP_MAX_EVENTS;
  for (uint8_t i = 0; i < count; i++) {
    if (strcmp(this->model_->events[i].name, name) == 0) {
      return static_cast<int16_t>(i);
    }
  }
  return -1;
}

const AxpAdcChannel *AxpComponent::find_adc_channel(const char *name) const {
  if (name == nullptr) {
    return nullptr;
  }
  for (uint8_t i = 0; i < this->model_->adc_count; i++) {
    if (strcmp(this->model_->adc_channels[i].name, name) == 0) {
      return &this->model_->adc_channels[i];
    }
  }
  return nullptr;
}

bool AxpComponent::set_rail_enabled(const AxpRail *rail, bool enabled) {
  if (rail == nullptr) {
    return false;
  }
  if (!axp_rail_has_enable(*rail)) {
    // Nothing to switch: the output is always present, so enabling it is a
    // no-op and disabling it is a configuration error.
    return enabled;
  }
  if (!enabled && (rail->flags & AXP_RAIL_ALWAYS_ON) != 0) {
    ESP_LOGE(TAG, "Refusing to disable %s, it is marked always on", rail->name);
    return false;
  }
  return this->update_reg_(rail->enable_reg, rail->enable_mask, enabled ? rail->enable_value : 0);
}

bool AxpComponent::is_rail_enabled(const AxpRail *rail) {
  if (rail == nullptr) {
    return false;
  }
  if (!axp_rail_has_enable(*rail)) {
    return true;
  }
  uint8_t value = 0;
  if (!this->read_reg_(rail->enable_reg, value)) {
    return false;
  }
  return (value & rail->enable_mask) == (rail->enable_value & rail->enable_mask);
}

bool AxpComponent::set_rail_voltage(const AxpRail *rail, uint16_t millivolt) {
  if (rail == nullptr || !axp_rail_has_voltage(*rail)) {
    return false;
  }
  uint8_t code = 0;
  if (!encode_voltage(*rail, millivolt, code)) {
    ESP_LOGE(TAG, "%s cannot produce %u mV", rail->name, static_cast<unsigned>(millivolt));
    return false;
  }
  return this->update_reg_(rail->vsel_reg, rail->vsel_mask, static_cast<uint8_t>(code << rail->vsel_shift));
}

bool AxpComponent::get_rail_voltage(const AxpRail *rail, uint16_t &millivolt) {
  if (rail == nullptr || !axp_rail_has_voltage(*rail)) {
    return false;
  }
  uint8_t value = 0;
  if (!this->read_reg_(rail->vsel_reg, value)) {
    return false;
  }
  return decode_voltage(*rail, static_cast<uint8_t>((value & rail->vsel_mask) >> rail->vsel_shift), millivolt);
}

bool AxpComponent::encode_voltage(const AxpRail &rail, uint16_t millivolt, uint8_t &code) {
  for (uint8_t i = 0; i < rail.range_count; i++) {
    const AxpVoltageRange &range = rail.ranges[i];
    if (range.step_mv == 0 || range.max_code < range.min_code || millivolt < range.min_mv) {
      continue;
    }
    const uint32_t delta = static_cast<uint32_t>(millivolt) - range.min_mv;
    if (delta % range.step_mv != 0) {
      continue;
    }
    const uint32_t index = delta / range.step_mv;
    if (index > static_cast<uint32_t>(range.max_code - range.min_code)) {
      continue;
    }
    code = static_cast<uint8_t>(range.min_code + index);
    return true;
  }
  return false;
}

bool AxpComponent::decode_voltage(const AxpRail &rail, uint8_t code, uint16_t &millivolt) {
  for (uint8_t i = 0; i < rail.range_count; i++) {
    const AxpVoltageRange &range = rail.ranges[i];
    if (code < range.min_code || code > range.max_code) {
      continue;
    }
    millivolt = range.min_mv + range.step_mv * (code - range.min_code);
    return true;
  }
  return false;
}

bool AxpComponent::encode_voltage_nearest(const AxpRail &rail, uint16_t millivolt, uint8_t &code, uint16_t &applied) {
  bool found = false;
  uint16_t best_delta = 0;

  for (uint8_t i = 0; i < rail.range_count; i++) {
    const AxpVoltageRange &range = rail.ranges[i];
    if (range.step_mv == 0 || range.max_code < range.min_code) {
      continue;
    }

    // Nearest index inside this window, clamped to its ends, so a value below
    // or above a window still yields that window's closest voltage.
    int32_t index = 0;
    if (millivolt > range.min_mv) {
      index = (static_cast<int32_t>(millivolt) - range.min_mv + range.step_mv / 2) / range.step_mv;
    }
    const int32_t max_index = static_cast<int32_t>(range.max_code) - range.min_code;
    if (index > max_index) {
      index = max_index;
    }
    if (index < 0) {
      index = 0;
    }

    const uint16_t candidate = range.min_mv + static_cast<uint16_t>(index) * range.step_mv;
    const uint16_t delta = absolute_difference(candidate, millivolt);
    if (!found || delta < best_delta) {
      found = true;
      best_delta = delta;
      code = static_cast<uint8_t>(range.min_code + index);
      applied = candidate;
    }
  }

  return found;
}

bool AxpComponent::set_rail_voltage_nearest(const AxpRail *rail, uint16_t millivolt, uint16_t *applied) {
  if (rail == nullptr || !axp_rail_has_voltage(*rail)) {
    return false;
  }
  uint8_t code = 0;
  uint16_t actual = 0;
  if (!encode_voltage_nearest(*rail, millivolt, code, actual)) {
    return false;
  }
  if (applied != nullptr) {
    *applied = actual;
  }
  return this->update_reg_(rail->vsel_reg, rail->vsel_mask, static_cast<uint8_t>(code << rail->vsel_shift));
}

uint16_t AxpComponent::count_rail_levels(const AxpRail &rail, uint16_t min_mv, uint16_t max_mv) {
  uint16_t count = 0;
  for (uint8_t i = 0; i < rail.range_count; i++) {
    const AxpVoltageRange &range = rail.ranges[i];
    if (range.step_mv == 0 || range.max_code < range.min_code) {
      continue;
    }
    for (uint16_t code = range.min_code; code <= range.max_code; code++) {
      const uint32_t millivolt = range.min_mv + static_cast<uint32_t>(range.step_mv) * (code - range.min_code);
      if (millivolt >= min_mv && millivolt <= max_mv) {
        count++;
      }
    }
  }
  return count;
}

bool AxpComponent::set_rail_dvm(const AxpRail *rail, bool enabled) {
  if (rail == nullptr || (rail->flags & AXP_RAIL_HAS_DVM) == 0) {
    return false;
  }
  // The DVM bit sits directly above the voltage code, so a voltage write with
  // vsel_mask leaves it untouched.
  return this->update_reg_(rail->vsel_reg, AXP_RAIL_DVM_MASK, enabled ? AXP_RAIL_DVM_MASK : 0);
}

float AxpComponent::read_adc(const AxpAdcChannel &channel) {
  uint8_t high = 0;
  uint8_t low = 0;
  if (!this->read_reg_(channel.hi_reg, high) || !this->read_reg_(channel.lo_reg, low)) {
    return NAN;
  }

  uint32_t raw;
  if (channel.mid_reg != AXP_REG_NONE) {
    uint8_t middle = 0;
    if (!this->read_reg_(channel.mid_reg, middle)) {
      return NAN;
    }
    raw = (static_cast<uint32_t>(high) << 16) | (static_cast<uint32_t>(middle) << 8) | low;
  } else {
    uint8_t low_bits = channel.lo_bits;
    if (low_bits == 0 || low_bits > 8) {
      low_bits = 8;
    }
    raw = (static_cast<uint32_t>(high & channel.hi_mask) << low_bits) |
          (static_cast<uint32_t>(low & channel.lo_mask) >> (8 - low_bits));
  }

  return static_cast<float>(raw) * channel.lsb + channel.offset;
}

bool AxpComponent::set_adc_enabled(const AxpAdcChannel &channel, bool enabled) {
  if (channel.enable_reg == AXP_REG_NONE) {
    return true;
  }
  const uint8_t bit = static_cast<uint8_t>(1u << channel.enable_bit);
  return this->update_reg_(channel.enable_reg, bit, enabled ? bit : 0);
}

bool AxpComponent::update_status() {
  const uint8_t count = this->model_->status_count;
  if (count == 0 || count > AXP_MAX_STATUS_REGS) {
    return false;
  }
  if (!this->read_regs_(this->model_->status_base, this->status_, count)) {
    this->status_valid_ = false;
    return false;
  }
  this->status_valid_ = true;
  this->status_clear_warning();
  return true;
}

bool AxpComponent::status_bit(const AxpStatusBit &bit) const {
  if (!this->status_valid_ || bit.mask == 0 || bit.reg >= this->model_->status_count ||
      bit.reg >= AXP_MAX_STATUS_REGS) {
    return false;
  }
  return (this->status_[bit.reg] & bit.mask) == bit.value;
}

bool AxpComponent::write_lut_value(const AxpLutField &field, uint16_t value, uint16_t *applied) {
  if (field.entries == nullptr || field.count == 0) {
    return false;
  }

  const AxpLutEntry *best = &field.entries[0];
  uint16_t best_delta = absolute_difference(best->value, value);
  for (uint8_t i = 1; i < field.count; i++) {
    const uint16_t delta = absolute_difference(field.entries[i].value, value);
    if (delta < best_delta) {
      best = &field.entries[i];
      best_delta = delta;
    }
  }

  if (applied != nullptr) {
    *applied = best->value;
  }
  return this->update_reg_(field.reg, field.mask, static_cast<uint8_t>(best->code << field.shift));
}

bool AxpComponent::read_lut_value(const AxpLutField &field, uint16_t &value) {
  if (field.entries == nullptr || field.count == 0) {
    return false;
  }

  uint8_t code = 0;
  if (!this->read_field_(AxpBitField{field.reg, field.mask, field.shift}, code)) {
    return false;
  }
  for (uint8_t i = 0; i < field.count; i++) {
    if (field.entries[i].code == code) {
      value = field.entries[i].value;
      return true;
    }
  }
  return false;
}

bool AxpComponent::set_events_enabled(uint64_t mask) {
  const uint8_t count = this->model_->irq_status_count;
  if (count == 0 || count > AXP_MAX_IRQ_REGS) {
    return false;
  }

  uint8_t wanted[AXP_MAX_IRQ_REGS] = {0};
  const uint8_t events = this->model_->event_count < AXP_MAX_EVENTS ? this->model_->event_count : AXP_MAX_EVENTS;
  for (uint8_t i = 0; i < events; i++) {
    if ((mask & (uint64_t{1} << i)) == 0) {
      continue;
    }
    const AxpEvent &event = this->model_->events[i];
    if (event.enable_index < count) {
      wanted[event.enable_index] |= static_cast<uint8_t>(1u << event.enable_bit);
    }
  }

  bool ok = true;
  for (uint8_t i = 0; i < count; i++) {
    ok &= this->update_reg_(this->model_->irq_enable_base + i, 0xFF, wanted[i]);
  }
  return ok;
}

bool AxpComponent::add_events_enabled(uint64_t mask) {
  const uint64_t wanted = this->events_enabled_mask_ | mask;
  if (wanted == this->events_enabled_mask_) {
    return true;
  }
  const bool first = this->events_enabled_mask_ == 0;
  this->events_enabled_mask_ = wanted;
  if (!this->set_events_enabled(wanted)) {
    return false;
  }
  if (first) {
    // The chip latches events whether or not anybody listens, so whatever is
    // pending now happened before this subscriber existed and must not be
    // reported as if it just happened.
    uint64_t discarded = 0;
    this->read_and_clear_events(discarded);
  }
  return true;
}

bool AxpComponent::read_and_clear_events(uint64_t &mask) {
  mask = 0;
  const uint8_t count = this->model_->irq_status_count;
  if (count == 0 || count > AXP_MAX_IRQ_REGS) {
    return false;
  }

  uint8_t status[AXP_MAX_IRQ_REGS] = {0};
  if (!this->read_regs_(this->model_->irq_status_base, status, count)) {
    return false;
  }

  bool pending = false;
  for (uint8_t i = 0; i < count; i++) {
    pending |= status[i] != 0;
  }
  // The status registers are write-one-to-clear, so writing back exactly what
  // was read clears the observed bits and nothing else.
  if (pending && !this->write_regs_(this->model_->irq_status_base, status, count)) {
    return false;
  }

  const uint8_t events = this->model_->event_count < AXP_MAX_EVENTS ? this->model_->event_count : AXP_MAX_EVENTS;
  for (uint8_t i = 0; i < events; i++) {
    const AxpEvent &event = this->model_->events[i];
    if (event.status_index < count && (status[event.status_index] & (1u << event.bit)) != 0) {
      mask |= uint64_t{1} << i;
    }
  }
  return true;
}

const AxpFlag *AxpComponent::find_flag(const char *name) const {
  if (name == nullptr) {
    return nullptr;
  }
  for (uint8_t i = 0; i < this->model_->flag_count; i++) {
    if (strcmp(this->model_->flags[i].name, name) == 0) {
      return &this->model_->flags[i];
    }
  }
  return nullptr;
}

bool AxpComponent::read_flag(const AxpFlag &flag, bool &on) {
  uint8_t code = 0;
  if (!this->read_field_(flag.field, code)) {
    return false;
  }
  on = code != 0;
  return true;
}

bool AxpComponent::write_flag(const AxpFlag &flag, bool on) { return this->write_field_(flag.field, on ? 1 : 0); }

bool AxpComponent::set_charging_enabled(bool enabled) {
  const AxpFlag *flag = this->find_flag("charging");
  return flag != nullptr && this->write_flag(*flag, enabled);
}

bool AxpComponent::is_charging_enabled() {
  const AxpFlag *flag = this->find_flag("charging");
  bool on = false;
  // A chip without the flag, or a failed read, is reported as not charging.
  return flag != nullptr && this->read_flag(*flag, on) && on;
}

bool AxpComponent::set_charge_current(uint16_t milliamps, uint16_t *applied) {
  const AxpSelect *select = this->find_select("charge_current");
  return select != nullptr && this->write_lut_value(select->field, milliamps, applied);
}

bool AxpComponent::get_charge_current(uint16_t &milliamps) {
  const AxpSelect *select = this->find_select("charge_current");
  return select != nullptr && this->read_lut_value(select->field, milliamps);
}

bool AxpComponent::set_charge_voltage(uint16_t millivolt, uint16_t *applied) {
  const AxpSelect *select = this->find_select("charge_voltage");
  return select != nullptr && this->write_lut_value(select->field, millivolt, applied);
}

bool AxpComponent::get_charge_voltage(uint16_t &millivolt) {
  const AxpSelect *select = this->find_select("charge_voltage");
  return select != nullptr && this->read_lut_value(select->field, millivolt);
}

const AxpSelect *AxpComponent::find_select(const char *name) const {
  if (name == nullptr) {
    return nullptr;
  }
  for (uint8_t i = 0; i < this->model_->select_count; i++) {
    if (strcmp(this->model_->selects[i].name, name) == 0) {
      return &this->model_->selects[i];
    }
  }
  return nullptr;
}

bool AxpComponent::read_select_code(const AxpSelect &select, uint8_t &code) {
  const AxpLutField &field = select.field;
  if (field.entries == nullptr || field.count == 0) {
    return false;
  }
  return this->read_field_({field.reg, field.mask, field.shift}, code);
}

bool AxpComponent::write_select_index(const AxpSelect &select, uint8_t index) {
  const AxpLutField &field = select.field;
  if (field.entries == nullptr || index >= field.count) {
    return false;
  }
  return this->write_field_({field.reg, field.mask, field.shift}, field.entries[index].code);
}

bool AxpComponent::set_gauge_enabled(bool enabled) {
  const AxpFlag *flag = this->find_flag("gauge");
  return flag != nullptr && this->write_flag(*flag, enabled);
}

bool AxpComponent::is_gauge_enabled() {
  const AxpFlag *flag = this->find_flag("gauge");
  bool on = false;
  return flag != nullptr && this->read_flag(*flag, on) && on;
}

bool AxpComponent::trigger_control_(const AxpBitField &field, const char *action) {
  if (field.reg == AXP_REG_NONE) {
    ESP_LOGW(TAG, "%s cannot %s", this->model_->name, action);
    return false;
  }
  // Read-modify-write so the configuration bits that share the register
  // survive, then write the action bit. The chip clears it again by itself.
  return this->update_reg_(field.reg, field.mask, field.mask);
}

bool AxpComponent::power_off() {
  if (!this->trigger_control_(this->model_->power_off, LOG_STR_LITERAL("power off"))) {
    return false;
  }
  ESP_LOGI(TAG, "Powering off");
  return true;
}

bool AxpComponent::restart() {
  if (!this->trigger_control_(this->model_->restart, LOG_STR_LITERAL("restart"))) {
    return false;
  }
  ESP_LOGI(TAG, "Restarting the SoC through the PMIC");
  return true;
}

bool AxpComponent::reset_gauge() {
  if (!this->trigger_control_(this->model_->gauge_reset, LOG_STR_LITERAL("reset the fuel gauge"))) {
    return false;
  }
  ESP_LOGI(TAG, "Fuel gauge reset");
  return true;
}

bool AxpComponent::read_state_of_charge(uint8_t &percent) {
  const uint8_t reg = this->model_->gauge_soc_reg;
  if (reg == AXP_REG_NONE) {
    return false;
  }
  uint8_t value = 0;
  if (!this->read_reg_(reg, value)) {
    return false;
  }
  // Anything above 100 means the gauge is not reporting a usable value, which
  // happens when no battery is connected.
  if (value > 100) {
    return false;
  }
  percent = value;
  return true;
}

bool AxpComponent::read_reg_(uint8_t reg, uint8_t &value) {
  if (this->read_register(reg, &value, 1) != i2c::ERROR_OK) {
    this->report_error_(LOG_STR("I2C read failed"));
    return false;
  }
  return true;
}

bool AxpComponent::write_reg_(uint8_t reg, uint8_t value) {
  if (this->write_register(reg, &value, 1) != i2c::ERROR_OK) {
    this->report_error_(LOG_STR("I2C write failed"));
    return false;
  }
  return true;
}

bool AxpComponent::update_reg_(uint8_t reg, uint8_t mask, uint8_t value) {
  uint8_t current = 0;
  if (!this->read_reg_(reg, current)) {
    return false;
  }
  const uint8_t updated = static_cast<uint8_t>((current & ~mask) | (value & mask));
  if (updated == current) {
    return true;
  }
  return this->write_reg_(reg, updated);
}

bool AxpComponent::read_regs_(uint8_t reg, uint8_t *data, size_t length) {
  if (this->read_register(reg, data, length) != i2c::ERROR_OK) {
    this->report_error_(LOG_STR("I2C read failed"));
    return false;
  }
  return true;
}

bool AxpComponent::write_regs_(uint8_t reg, const uint8_t *data, size_t length) {
  if (this->write_register(reg, data, length) != i2c::ERROR_OK) {
    this->report_error_(LOG_STR("I2C write failed"));
    return false;
  }
  return true;
}

const AxpNumber *AxpComponent::find_number(const char *name) const {
  if (name == nullptr) {
    return nullptr;
  }
  for (uint8_t i = 0; i < this->model_->number_count; i++) {
    if (strcmp(this->model_->numbers[i].name, name) == 0) {
      return &this->model_->numbers[i];
    }
  }
  return nullptr;
}

bool AxpComponent::read_number_code(const AxpNumber &number, uint8_t &code) {
  return this->read_field_(number.field, code);
}

bool AxpComponent::write_number_code(const AxpNumber &number, uint8_t code) {
  if (code < number.min_code || code > number.max_code) {
    return false;
  }
  return this->write_field_(number.field, code);
}

int32_t AxpComponent::decode_number(const AxpNumber &number, uint8_t code) {
  return number.offset + static_cast<int32_t>(number.step) * code;
}

uint8_t AxpComponent::encode_number(const AxpNumber &number, int32_t value) {
  if (number.step == 0) {
    return number.min_code;
  }
  // Round to the closest code, in either direction, then stay inside the codes
  // the setting defines.
  const int32_t delta = value - number.offset;
  const int32_t half = number.step / 2;
  const int32_t code = delta >= 0 ? (delta + half) / number.step : -((-delta + half) / number.step);
  if (code < number.min_code) {
    return number.min_code;
  }
  if (code > number.max_code) {
    return number.max_code;
  }
  return static_cast<uint8_t>(code);
}

bool AxpComponent::read_field_(const AxpBitField &field, uint8_t &code) {
  uint8_t value = 0;
  if (!this->read_reg_(field.reg, value)) {
    return false;
  }
  code = static_cast<uint8_t>((value & field.mask) >> field.shift);
  return true;
}

bool AxpComponent::write_field_(const AxpBitField &field, uint8_t code) {
  return this->update_reg_(field.reg, field.mask, static_cast<uint8_t>(code << field.shift));
}

void AxpComponent::report_error_(const LogString *message) {
  // Keep the first failure only: an unplugged bus would otherwise log on every
  // register access until the next successful read clears the warning.
  if (this->status_has_warning()) {
    return;
  }
  ESP_LOGW(TAG, "%s", LOG_STR_ARG(message));
  this->status_set_warning(message);
}

void IRAM_ATTR AxpComponent::irq_isr_(AxpComponent *arg) { arg->irq_pending_ = true; }

}  // namespace esphome::axp_base
