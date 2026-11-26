#include "m5_unit_acssr.h"

#include <cstring>

#include "esphome/core/log.h"

namespace esphome::m5_unit_acssr {

ESPHOME_LOG_TAG(TAG, "m5_unit_acssr");

// The unit starts its I2C target about 3 s after power on, so a unit powered with the board misses setup()
static constexpr uint32_t CONNECT_RETRY_MS = 1000;
static constexpr uint8_t CONNECT_RETRIES = 10;

void M5UnitACSSR::setup() {
#ifdef USE_SWITCH
  if (this->relay_switch_ != nullptr) {
    auto initial = this->relay_switch_->get_initial_state_with_restore_mode();
    if (initial.has_value()) {
      this->relay_on_ = *initial != this->relay_switch_->is_inverted();
      this->relay_known_ = true;
    }
  }
#endif
  if (this->connect_())
    return;
  this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
  this->retrying_ = true;
  this->set_interval(CONNECT_INTERVAL_ID, CONNECT_RETRY_MS, [this]() { this->retry_connect_(); });
}

void M5UnitACSSR::retry_connect_() {
  const bool connected = this->connect_();
  if (!connected && ++this->connect_attempts_ < CONNECT_RETRIES)
    return;
  this->retrying_ = false;
  this->cancel_interval(CONNECT_INTERVAL_ID);
  if (connected)
    return;
  // From here update() keeps trying at the update interval
  ESP_LOGW(TAG, "Unit not answering at 0x%02X; check it is in I2C mode (its LED flashes red at power on)",
           this->address_);
}

bool M5UnitACSSR::connect_() {
  if (!this->read_register_(REG_FIRMWARE_VERSION, &this->firmware_version_, 1))
    return false;
  if (!this->relay_known_) {
    uint8_t relay{0};
    if (!this->read_register_(REG_RELAY, &relay, 1))
      return false;
    this->relay_on_ = relay != 0;
    this->relay_known_ = true;
  }
  if (!this->write_state_())
    return false;
  this->connected_ = true;
  this->status_clear_warning();
#ifdef USE_SWITCH
  if (this->relay_switch_ != nullptr)
    this->relay_switch_->publish_state(this->relay_on_);
#endif
  return true;
}

bool M5UnitACSSR::write_state_() {
  const uint8_t relay = this->relay_on_ ? 1 : 0;
  return this->write_bytes(REG_RELAY, &relay, 1) && this->write_bytes(REG_LED, this->led_, sizeof(this->led_));
}

void M5UnitACSSR::update() {
  if (!this->connected_) {
    if (!this->retrying_)
      this->connect_();
    return;
  }
  uint8_t relay{0};
  uint8_t led[3]{};
  if (!this->read_register_(REG_RELAY, &relay, 1) || !this->read_register_(REG_LED, led, sizeof(led))) {
    this->lost_();
    return;
  }
  // Only the hub changes these in I2C mode, so a difference means the unit restarted
  if ((relay != 0) == this->relay_on_ && std::memcmp(led, this->led_, sizeof(led)) == 0)
    return;
  ESP_LOGW(TAG, "Unit lost its relay and LED state, probably in a restart; restoring them");
  if (!this->write_state_())
    this->lost_();
}

bool M5UnitACSSR::write_relay(bool on) {
  if (!this->connected_)
    return false;
  const uint8_t value = on ? 1 : 0;
  if (!this->write_bytes(REG_RELAY, &value, 1)) {
    this->lost_();
    return false;
  }
  this->relay_on_ = on;
  this->relay_known_ = true;
#ifdef USE_SWITCH
  if (this->relay_switch_ != nullptr)
    this->relay_switch_->publish_state(on);
#endif
  return true;
}

bool M5UnitACSSR::write_led(uint8_t red, uint8_t green, uint8_t blue) {
  // Held while the unit is away, then written when it answers again
  this->led_[0] = red;
  this->led_[1] = green;
  this->led_[2] = blue;
  if (!this->connected_)
    return false;
  if (!this->write_bytes(REG_LED, this->led_, sizeof(this->led_))) {
    this->lost_();
    return false;
  }
  return true;
}

void M5UnitACSSR::lost_() {
  this->connected_ = false;
  this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
}

bool M5UnitACSSR::read_register_(uint8_t reg, uint8_t *data, size_t len) {
  // Select the register in its own transfer, as the vendor library does
  return this->write(&reg, 1) == i2c::ERROR_OK && this->read(data, len) == i2c::ERROR_OK;
}

void M5UnitACSSR::dump_config() {
  ESP_LOGCONFIG(TAG,
                "M5Stack Unit ACSSR:\n"
                "  Firmware version: %u",
                this->firmware_version_);
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
#ifdef USE_SWITCH
  LOG_SWITCH("  ", "Relay", this->relay_switch_);
#endif
}

}  // namespace esphome::m5_unit_acssr
