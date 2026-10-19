#include "unit_scroll_sensor.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace unit_scroll {

static const char *const TAG = "unit_scroll";

void UnitScrollComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Unit Scroll...");
  
  // Test if device is connected
  if (!this->read_register_(SCROLL_BOOTLOADER_VERSION_REG, nullptr, 0)) {
    ESP_LOGE(TAG, "Communication with Unit Scroll failed!");
    this->mark_failed();
    return;
  }
  
  uint8_t bootloader_version = 0;
  uint8_t firmware_version = 0;
  this->read_register_(SCROLL_BOOTLOADER_VERSION_REG, &bootloader_version, 1);
  this->read_register_(SCROLL_FIRMWARE_VERSION_REG, &firmware_version, 1);
  ESP_LOGD(TAG, "Unit Scroll bootloader version: 0x%02X", bootloader_version);
  ESP_LOGD(TAG, "Unit Scroll firmware version: 0x%02X", firmware_version);
  
  // Apply initial configuration
  ESP_LOGD(TAG, "Applying initial configuration...");
  
  // Configure RGB
  this->set_rgb(this->initial_rgb_r_, this->initial_rgb_g_, this->initial_rgb_b_);
  ESP_LOGD(TAG, "RGB color set to: (%d,%d,%d)", 
           this->initial_rgb_r_, this->initial_rgb_g_, this->initial_rgb_b_);

  this->set_direction(this->initial_direction_);
  ESP_LOGD(TAG, "Direction set to: %s", this->initial_direction_ ? "BA" : "AB");
}

void UnitScrollComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Unit Scroll:");
  LOG_I2C_DEVICE(this);
  
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with Unit Scroll failed!");
    return;
  }
  
  uint8_t bootloader_version = 0;
  uint8_t firmware_version = 0;
  this->read_register_(SCROLL_BOOTLOADER_VERSION_REG, &bootloader_version, 1);
  this->read_register_(SCROLL_FIRMWARE_VERSION_REG, &firmware_version, 1);
  ESP_LOGCONFIG(TAG, "  Bootloader Version: 0x%02X", bootloader_version);
  ESP_LOGCONFIG(TAG, "  Firmware Version: 0x%02X", firmware_version);
}

bool UnitScrollComponent::read_register_(uint8_t reg_addr, uint8_t *data, uint8_t len) {
  if (len == 0) {
    // Just test communication
    return this->write(&reg_addr, 1) == i2c::ERROR_OK;
  }

  if (this->write(&reg_addr, 1) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Failed to write register address 0x%02X", reg_addr);
    return false;
  }

  if (this->read(data, len) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Failed to read %d bytes from register 0x%02X", len, reg_addr);
    return false;
  }

  return true;
}

bool UnitScrollComponent::write_register_(uint8_t reg_addr, uint8_t *data, uint8_t len) {
  uint8_t buffer[len + 1];
  buffer[0] = reg_addr;
  memcpy(&buffer[1], data, len);

  if (this->write(buffer, len + 1) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Failed to write %d bytes to register 0x%02X", len, reg_addr);
    return false;
  }

  return true;
}

uint16_t UnitScrollComponent::get_value() {
  uint8_t data[2] = {0, 0};
  if (!this->read_register_(SCROLL_ENCODER_REG, data, 2)) {
    ESP_LOGE(TAG, "Failed to read encoder value");
    return 0;
  }
  uint16_t value = static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
  return value;
}

bool UnitScrollComponent::set_value(uint16_t value) {
  uint8_t data[2] = {static_cast<uint8_t>(value & 0xFF), static_cast<uint8_t>((value >> 8) & 0xFF)};
  return this->write_register_(SCROLL_ENCODER_REG, data, 2);
}

uint16_t UnitScrollComponent::get_inc_value() {
  uint8_t data[2] = {0, 0};
  if (!this->read_register_(SCROLL_INC_ENCODER_REG, data, 2)) {
    ESP_LOGE(TAG, "Failed to read incremental encoder value");
    return 0;
  }
  uint16_t value = static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
  return value;
}

bool UnitScrollComponent::get_button_state() {
  uint8_t state = 0;
  this->read_register_(SCROLL_BTN_REG, &state, 1);
  return state != 0;
}

bool UnitScrollComponent::set_rgb(uint8_t r, uint8_t g, uint8_t b) {
  uint8_t rgb[4] = {0x00, r, g, b};
  return this->write_register_(SCROLL_RGB_CONFIG_REG, rgb, 4);
}

bool UnitScrollComponent::get_rgb(uint8_t *r, uint8_t *g, uint8_t *b) {
  uint8_t rgb[4];
  if (!this->read_register_(SCROLL_RGB_CONFIG_REG, rgb, 4)) {
    return false;
  }
  *r = rgb[1];
  *g = rgb[2];
  *b = rgb[3];
  return true;
}

bool UnitScrollComponent::reset_encoder() {
  uint8_t reset = 1;
  return this->write_register_(SCROLL_ENCODER_RST_REG, &reset, 1);
}

bool UnitScrollComponent::set_direction(bool dir) {
  uint8_t direction = dir ? 1 : 0;
  return this->write_register_(SCROLL_AB_BA_REG, &direction, 1);
}

bool UnitScrollComponent::get_direction() {
  uint8_t direction = 0;
  this->read_register_(SCROLL_AB_BA_REG, &direction, 1);
  return direction != 0;
}

uint8_t UnitScrollComponent::get_bootloader_version() {
  uint8_t version = 0;
  this->read_register_(SCROLL_BOOTLOADER_VERSION_REG, &version, 1);
  return version;
}

uint8_t UnitScrollComponent::get_firmware_version() {
  uint8_t version = 0;
  this->read_register_(SCROLL_FIRMWARE_VERSION_REG, &version, 1);
  return version;
}

void UnitScrollSensor::update() {
  if (this->parent_ == nullptr) {
    ESP_LOGW(TAG, "Unit Scroll parent is not configured");
    return;
  }

  this->publish_state(this->parent_->get_value());
}

void UnitScrollSensor::dump_config() {
  LOG_SENSOR("", "Unit Scroll Sensor", this);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace unit_scroll
}  // namespace esphome
