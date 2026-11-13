#include "aw88298.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::aw88298 {

ESPHOME_LOG_TAG(TAG, "aw88298");

/// The datasheet asks for more than 1 ms between enabling the system and the amplifier.
static constexpr uint32_t START_DELAY_MS = 2;
/// RSTN low for 10 ms, then 50 ms before the device answers on the bus again. These are the
/// timings Espressif's AW88298 driver uses for this chip.
static constexpr uint32_t RESET_LOW_MS = 10;
static constexpr uint32_t RESET_RECOVER_MS = 50;

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_CONFIG
PROGMEM_STRING_TABLE(I2SFormatStrings, "i2s", "left justified", "right justified", "unknown");
PROGMEM_STRING_TABLE(BoostModeStrings, "pass-through", "force boost", "smart boost 1", "smart boost 2", "unknown");
static const LogString *channel_name(ChannelSelect channel) {
  switch (channel) {
    case ChannelSelect::CHANNEL_SELECT_RIGHT:
      return LOG_STR("right");
    case ChannelSelect::CHANNEL_SELECT_MONO:
      return LOG_STR("mono");
    default:
      return LOG_STR("left");
  }
}
#endif

PROGMEM_STRING_TABLE(SettingStrings, "hdcc", "agc", "agc rms", "input attenuation", "receiver mode", "boost mode",
                     "boost deglitch", "boost voltage", "boost current limit", "boost attack threshold",
                     "boost release threshold", "agc attack threshold", "agc release threshold", "agc attack time",
                     "agc release time", "agc hold time", "unknown");

const LogString *setting_name(Setting setting) {
  return SettingStrings::get_log_str(static_cast<uint8_t>(setting), SettingStrings::LAST_INDEX);
}

void AW88298::setup() {
  ESP_LOGCONFIG(TAG, "Setting up AW88298...");

  if (this->reset_pin_ != nullptr)
    this->reset_();

  uint16_t id_code{0};
  if (!this->read_byte_16(REG_ID, &id_code)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  if (id_code != IDCODE_AW88298) {
    ESP_LOGE(TAG, "Chip ID 0x%04X is not an AW88298", id_code);
    this->mark_failed(LOG_STR("Wrong chip ID"));
    return;
  }

  // Writing 0x55AA to IDCODE resets every configuration register to its default.
  if (!this->write_byte_16(REG_ID, RESET_CODE)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  // Everything that the I2S frame decides goes in first, while the device is still powered down.
  if (!this->write_i2s_config_()) {
    this->mark_failed(LOG_STR("Unsupported I2S configuration"));
    return;
  }

  const uint16_t hagccfg1 =
      static_cast<uint16_t>(agc_threshold_reg(this->get_setting(Setting::SETTING_AGC_RELEASE_THRESHOLD)) << 8 |
                            agc_threshold_reg(this->get_setting(Setting::SETTING_AGC_ATTACK_THRESHOLD)));
  const uint16_t bstctrl1 =
      static_cast<uint16_t>(boost_threshold_reg(this->get_setting(Setting::SETTING_BOOST_RELEASE_THRESHOLD)) << 8 |
                            boost_threshold_reg(this->get_setting(Setting::SETTING_BOOST_ATTACK_THRESHOLD)));
  uint8_t vout_vrefset;
  if (!boost_voltage_reg(this->get_setting(Setting::SETTING_BOOST_VOLTAGE), &vout_vrefset)) {
    this->mark_failed(LOG_STR("Boost voltage out of range"));
    return;
  }
  const uint16_t bstctrl2 = static_cast<uint16_t>(
      boost_mode_code(static_cast<uint8_t>(this->get_setting(Setting::SETTING_BOOST_MODE))) << 12 |
      (static_cast<uint8_t>(this->get_setting(Setting::SETTING_BOOST_DEGLITCH)) & 0x07) << 8 | vout_vrefset);

  bool ok = this->write_byte_16(REG_HAGCCFG1, hagccfg1) &&
            this->write_byte_16(REG_HAGCCFG2,
                                agc_time_reg(this->get_setting(Setting::SETTING_AGC_ATTACK_TIME), AGC_TIME_UNIT_MS)) &&
            this->write_byte_16(REG_HAGCCFG3,
                                agc_time_reg(this->get_setting(Setting::SETTING_AGC_RELEASE_TIME), AGC_TIME_UNIT_MS)) &&
            this->write_byte_16(REG_BSTCTRL1, bstctrl1) && this->write_byte_16(REG_BSTCTRL2, bstctrl2) &&
            this->write_byte_16(REG_SYSCTRL2, this->sysctrl2_value_());
  if (!ok) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  if (!this->set_volume(this->volume_)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  // Power up sequence, Table 2: the system first, so the PLL can lock, then the amplifier, then
  // the hard mute. The speaker starts the clock later, which on_audio_started() checks for.
  if (!this->write_byte_16(REG_SYSCTRL, this->sysctrl_value_() | SYSCTRL_PWDN | SYSCTRL_AMPPD)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  delay(START_DELAY_MS);
  if (!this->write_byte_16(REG_SYSCTRL, this->sysctrl_value_())) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  delay(START_DELAY_MS);
  if (!this->set_mute_state_(false)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  this->initialized_ = true;
  ESP_LOGCONFIG(TAG, "Setup finished");
}

void AW88298::update() {
  if (this->is_failed())
    return;

  uint16_t status{0};
  if (!this->status_comm_result(this->read_byte_16(REG_SYSST, &status)))
    return;
  if ((status & SYSST_FAULT_MASK) != 0) {
    ESP_LOGW(TAG, "Device reports a fault, status 0x%04X", status);
  }

  this->publish_status_(status);
  this->publish_monitors_();
}

void AW88298::dump_config() {
  ESP_LOGCONFIG(TAG, "AW88298:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Reset pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG, "  Sample rate: %u Hz", static_cast<unsigned>(this->sample_rate_));
  ESP_LOGCONFIG(TAG, "  I2S: %u bit data, %u bit slots, %u slots", static_cast<unsigned>(this->data_bits_),
                static_cast<unsigned>(this->slot_bits_), static_cast<unsigned>(this->slot_count_));
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_CONFIG
  ESP_LOGCONFIG(
      TAG, "  Format: %s, channel: %s",
      LOG_STR_ARG(I2SFormatStrings::get_log_str(static_cast<uint8_t>(this->format_), I2SFormatStrings::LAST_INDEX)),
      LOG_STR_ARG(channel_name(this->channel_)));
#endif
  ESP_LOGCONFIG(TAG, "  Volume: %.1f dB to %.1f dB, start at %.0f%%", this->volume_min_db_, this->volume_max_db_,
                this->volume_ * 100.0f);
  ESP_LOGCONFIG(TAG, "  Boost: %.3f V, %.2f A limit", this->get_setting(Setting::SETTING_BOOST_VOLTAGE),
                this->get_setting(Setting::SETTING_BOOST_CURRENT_LIMIT));
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_CONFIG
  ESP_LOGCONFIG(
      TAG, "  Boost mode: %s",
      LOG_STR_ARG(BoostModeStrings::get_log_str(static_cast<uint8_t>(this->get_setting(Setting::SETTING_BOOST_MODE)),
                                                BoostModeStrings::LAST_INDEX)));
#endif
  ESP_LOGCONFIG(TAG, "  HDCC: %s, AGC: %s", YESNO(this->get_setting(Setting::SETTING_HDCC) > 0.0f),
                YESNO(this->get_setting(Setting::SETTING_AGC) > 0.0f));
}

/// Same contract as Component::status_comm_result() in ESPHome 2026.11: clear the warning on
/// success, set it on failure, and report the result.
bool AW88298::status_comm_result(bool ok) {
  if (ok) {
    this->status_clear_warning();
  } else {
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
  }
  return ok;
}

void AW88298::reset_() {
  this->reset_pin_->setup();
  this->reset_pin_->digital_write(false);
  delay(RESET_LOW_MS);
  this->reset_pin_->digital_write(true);
  delay(RESET_RECOVER_MS);
}

bool AW88298::set_volume(float volume) {
  const float clamped = clamp(volume, 0.0f, 1.0f);
  const float db = this->volume_min_db_ + clamped * (this->volume_max_db_ - this->volume_min_db_);
  const auto hold_time = agc_time_reg(this->get_setting(Setting::SETTING_AGC_HOLD_TIME), AGC_HOLD_TIME_UNIT_MS);

  if (!this->write_byte_16(REG_HAGCCFG4, static_cast<uint16_t>(volume_reg_from_db(db)) << 8 | hold_time)) {
    this->status_set_warning();
    return false;
  }
  this->status_clear_warning();
  this->volume_ = clamped;
  return true;
}

bool AW88298::set_mute_state_(bool mute_state) {
  if (!this->update_bits_(REG_SYSCTRL2, SYSCTRL2_HMUTE, mute_state ? SYSCTRL2_HMUTE : 0))
    return false;
  this->is_muted_ = mute_state;
  return true;
}

void AW88298::on_audio_started() {
  if (this->is_failed())
    return;

  uint16_t status{0};
  if (!this->status_comm_result(this->read_byte_16(REG_SYSST, &status)))
    return;
  // SWS only reads set once the boost and the class-D are running off a locked PLL, which needs
  // the clock the speaker has just started.
  if ((status & SYSST_AMPLIFIER_SWITCHING) == 0) {
    ESP_LOGW(TAG, "Amplifier is not switching yet, status 0x%04X", status);
    this->status_set_warning(LOG_STR("Amplifier did not start"));
  }
  this->publish_status_(status);
}

bool AW88298::write_i2s_config_() {
  I2SSettings settings{};
  if (!sample_rate_code(this->sample_rate_, &settings.sample_rate_code)) {
    ESP_LOGE(TAG, "Sample rate %u Hz is not one the AW88298 supports", static_cast<unsigned>(this->sample_rate_));
    return false;
  }
  settings.data_bits = this->data_bits_;
  settings.slot_bits = this->slot_bits_;
  settings.slot_count = this->slot_count_;
  settings.rx_slot_mask = this->rx_slot_mask_;
  settings.format = this->format_;
  settings.channel = this->channel_;
  settings.input_attenuation = this->get_setting(Setting::SETTING_INPUT_ATTENUATION) > 0.0f;
  settings.ws_pulse_one_bck = this->ws_pulse_one_bck_;

  uint16_t i2sctrl;
  if (!encode_i2sctrl(settings, &i2sctrl)) {
    ESP_LOGE(TAG, "No BCK ratio for %u bit data in %u slots of %u bits", static_cast<unsigned>(this->data_bits_),
             static_cast<unsigned>(this->slot_count_), static_cast<unsigned>(this->slot_bits_));
    return false;
  }
  return this->write_byte_16(REG_I2SCTRL, i2sctrl) && this->write_byte_16(REG_I2SCFG1, encode_i2scfg1(settings));
}

uint16_t AW88298::sysctrl_value_() const {
  uint16_t value = SYSCTRL_RESERVED | SYSCTRL_I2S_ENABLE;
  if (this->get_setting(Setting::SETTING_RECEIVER_MODE) > 0.0f)
    value |= SYSCTRL_RECEIVER_MODE;
  if (this->ws_invert_)
    value |= SYSCTRL_WS_INVERT;
  if (this->bclk_invert_)
    value |= SYSCTRL_BCLK_INVERT;
  if (this->pll_from_ws_)
    value |= SYSCTRL_PLL_FROM_WS;
  return value;
}

uint16_t AW88298::sysctrl2_value_() const {
  // Stay hard muted until setup has enabled the amplifier and released it.
  uint16_t value = SYSCTRL2_HMUTE;
  if (this->get_setting(Setting::SETTING_HDCC) > 0.0f)
    value |= SYSCTRL2_HDCCE;
  if (this->get_setting(Setting::SETTING_AGC) > 0.0f)
    value |= SYSCTRL2_HAGCE;
  if (this->get_setting(Setting::SETTING_AGC_RMS) > 0.0f)
    value |= SYSCTRL2_RMSE;
  uint8_t ipeak;
  if (boost_current_limit_reg(this->get_setting(Setting::SETTING_BOOST_CURRENT_LIMIT), &ipeak))
    value |= ipeak;
  return value;
}

bool AW88298::set_setting(Setting setting, float value) {
  const auto index = static_cast<uint8_t>(setting);
  const float previous = this->settings_[index];
  this->settings_[index] = value;
  if (!this->initialized_ || this->write_setting_(setting))
    return true;
  this->settings_[index] = previous;
  return false;
}

bool AW88298::write_setting_(Setting setting) {
  const float value = this->get_setting(setting);
  const auto index = static_cast<uint8_t>(setting);
  switch (setting) {
    case Setting::SETTING_HDCC:
      return this->update_bits_(REG_SYSCTRL2, SYSCTRL2_HDCCE, value > 0.0f ? SYSCTRL2_HDCCE : 0);
    case Setting::SETTING_AGC:
      return this->update_bits_(REG_SYSCTRL2, SYSCTRL2_HAGCE, value > 0.0f ? SYSCTRL2_HAGCE : 0);
    case Setting::SETTING_AGC_RMS:
      return this->update_bits_(REG_SYSCTRL2, SYSCTRL2_RMSE, value > 0.0f ? SYSCTRL2_RMSE : 0);
    case Setting::SETTING_INPUT_ATTENUATION:
      return this->update_bits_(REG_I2SCTRL, I2SCTRL_INPLEV, value > 0.0f ? I2SCTRL_INPLEV : 0);
    case Setting::SETTING_RECEIVER_MODE:
      return this->update_bits_(REG_SYSCTRL, SYSCTRL_RECEIVER_MODE, value > 0.0f ? SYSCTRL_RECEIVER_MODE : 0);
    case Setting::SETTING_BOOST_MODE:
      return this->update_bits_(REG_BSTCTRL2, BSTCTRL2_BST_MODE_MASK,
                                static_cast<uint16_t>(boost_mode_code(static_cast<uint8_t>(value))) << 12);
    case Setting::SETTING_BOOST_DEGLITCH:
      return this->update_bits_(REG_BSTCTRL2, BSTCTRL2_BST_TDEG_MASK,
                                static_cast<uint16_t>(static_cast<uint8_t>(value) & 0x07) << 8);
    case Setting::SETTING_BOOST_VOLTAGE: {
      uint8_t vout_vrefset;
      if (!boost_voltage_reg(value, &vout_vrefset) ||
          !this->update_bits_(REG_BSTCTRL2, BSTCTRL2_VOUT_VREFSET_MASK, vout_vrefset))
        return false;
      this->settings_[index] = boost_voltage_from_reg(vout_vrefset);
      return true;
    }
    case Setting::SETTING_BOOST_CURRENT_LIMIT: {
      uint8_t ipeak;
      if (!boost_current_limit_reg(value, &ipeak) || !this->update_bits_(REG_SYSCTRL2, SYSCTRL2_BST_IPEAK_MASK, ipeak))
        return false;
      this->settings_[index] = boost_current_limit_from_reg(ipeak);
      return true;
    }
    case Setting::SETTING_BOOST_ATTACK_THRESHOLD: {
      const uint8_t code = boost_threshold_reg(value);
      if (!this->update_bits_(REG_BSTCTRL1, BSTCTRL1_BST_ATH_MASK, code))
        return false;
      this->settings_[index] = boost_threshold_percent(code);
      return true;
    }
    case Setting::SETTING_BOOST_RELEASE_THRESHOLD: {
      const uint8_t code = boost_threshold_reg(value);
      if (!this->update_bits_(REG_BSTCTRL1, BSTCTRL1_BST_RTH_MASK, static_cast<uint16_t>(code) << 8))
        return false;
      this->settings_[index] = boost_threshold_percent(code);
      return true;
    }
    case Setting::SETTING_AGC_ATTACK_THRESHOLD: {
      const uint8_t code = agc_threshold_reg(value);
      if (!this->update_bits_(REG_HAGCCFG1, 0x00FF, code))
        return false;
      this->settings_[index] = code;
      return true;
    }
    case Setting::SETTING_AGC_RELEASE_THRESHOLD: {
      const uint8_t code = agc_threshold_reg(value);
      if (!this->update_bits_(REG_HAGCCFG1, 0xFF00, static_cast<uint16_t>(code) << 8))
        return false;
      this->settings_[index] = code;
      return true;
    }
    case Setting::SETTING_AGC_ATTACK_TIME:
    case Setting::SETTING_AGC_RELEASE_TIME: {
      const uint16_t code = agc_time_reg(value, AGC_TIME_UNIT_MS);
      const uint8_t reg = setting == Setting::SETTING_AGC_ATTACK_TIME ? REG_HAGCCFG2 : REG_HAGCCFG3;
      if (!this->write_byte_16(reg, code))
        return false;
      this->settings_[index] = agc_time_ms(code, AGC_TIME_UNIT_MS);
      return true;
    }
    case Setting::SETTING_AGC_HOLD_TIME: {
      // The hold time shares HAGCCFG4 with the volume, so only the low byte is rewritten.
      uint16_t current;
      if (!this->read_byte_16(REG_HAGCCFG4, &current))
        return false;
      const uint16_t code = agc_time_reg(value, AGC_HOLD_TIME_UNIT_MS);
      if (!this->write_byte_16(REG_HAGCCFG4, static_cast<uint16_t>(current & 0xFF00) | code))
        return false;
      this->settings_[index] = agc_time_ms(code, AGC_HOLD_TIME_UNIT_MS);
      return true;
    }
    default:
      return false;
  }
}

bool AW88298::update_bits_(uint8_t reg, uint16_t mask, uint16_t value) {
  uint16_t current;
  if (!this->read_byte_16(reg, &current)) {
    this->status_set_warning();
    return false;
  }
  const uint16_t updated = static_cast<uint16_t>((current & ~mask) | (value & mask));
  if (updated != current && !this->write_byte_16(reg, updated)) {
    this->status_set_warning();
    return false;
  }
  this->status_clear_warning();
  return true;
}

void AW88298::publish_status_(uint16_t status) {
#ifdef AW88298_STATUS_SENSOR_COUNT
  for (auto &entry : this->status_sensors_) {
    const bool state = entry.flag == StatusFlag::STATUS_FLAG_FAULT ? (status & SYSST_FAULT_MASK) != 0
                                                                   : (status & status_bit(entry.flag)) != 0;
    entry.sensor->publish_state(state);
  }
#endif
}

void AW88298::publish_monitors_() {
#ifdef AW88298_SENSOR_COUNT
  for (uint8_t type = 0; type < static_cast<uint8_t>(SensorType::SENSOR_TYPE_COUNT); type++) {
    bool wanted = false;
    for (auto &entry : this->sensors_) {
      if (static_cast<uint8_t>(entry.type) == type) {
        wanted = true;
        break;
      }
    }
    if (!wanted)
      continue;

    float value;
    if (!this->read_monitor_(static_cast<SensorType>(type), &value))
      continue;
    for (auto &entry : this->sensors_) {
      if (static_cast<uint8_t>(entry.type) == type)
        entry.sensor->publish_state(value);
    }
  }
#endif
}

bool AW88298::read_monitor_(SensorType type, float *value) {
  uint16_t raw{0};
  switch (type) {
    case SensorType::SENSOR_TYPE_DIE_TEMPERATURE:
      if (!this->read_byte_16(REG_TEMP, &raw))
        return false;
      *value = temperature_from_reg(raw);
      return true;
    case SensorType::SENSOR_TYPE_SUPPLY_VOLTAGE:
      if (!this->read_byte_16(REG_VDD, &raw))
        return false;
      *value = vdd_from_reg(raw);
      return true;
    case SensorType::SENSOR_TYPE_AMPLIFIER_VOLTAGE:
      if (!this->read_byte_16(REG_PVDD, &raw))
        return false;
      *value = pvdd_from_reg(raw);
      return true;
    case SensorType::SENSOR_TYPE_BOOST_VOLTAGE:
      if (!this->read_byte_16(REG_HAGCST, &raw))
        return false;
      *value = boost_status_voltage_from_reg(raw);
      return true;
    default:
      return false;
  }
}

#ifdef AW88298_SENSOR_COUNT
void AW88298::add_sensor(SensorType type, sensor::Sensor *sensor) { this->sensors_.push_back({type, sensor}); }
#endif

#ifdef AW88298_STATUS_SENSOR_COUNT
void AW88298::add_status_binary_sensor(StatusFlag flag, binary_sensor::BinarySensor *sensor) {
  this->status_sensors_.push_back({flag, sensor});
}
#endif

}  // namespace esphome::aw88298
