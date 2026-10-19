#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace unit_scroll {

// Register addresses
static const uint8_t SCROLL_ENCODER_REG = 0x10;
static const uint8_t SCROLL_BTN_REG = 0x20;
static const uint8_t SCROLL_RGB_CONFIG_REG = 0x30;
static const uint8_t SCROLL_ENCODER_RST_REG = 0x40;
static const uint8_t SCROLL_INC_ENCODER_REG = 0x50;
static const uint8_t SCROLL_AB_BA_REG = 0xFB;
static const uint8_t SCROLL_BOOTLOADER_VERSION_REG = 0xFC;
static const uint8_t SCROLL_FIRMWARE_VERSION_REG = 0xFE;
static const uint8_t SCROLL_ADDRESS_REG = 0xFF;

class UnitScrollComponent : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Set initial values (called during configuration)
  void set_initial_direction(bool dir) { initial_direction_ = dir; }
  void set_initial_rgb_color(uint8_t r, uint8_t g, uint8_t b) {
    initial_rgb_r_ = r;
    initial_rgb_g_ = g;
    initial_rgb_b_ = b;
  }

  // Read 16-bit encoder value
  uint16_t get_value();
  bool set_value(uint16_t value);
  uint16_t get_inc_value();

  // Button state
  bool get_button_state();

  // RGB control
  bool set_rgb(uint8_t r, uint8_t g, uint8_t b);
  bool get_rgb(uint8_t *r, uint8_t *g, uint8_t *b);

  // Encoder control
  bool reset_encoder();
  bool set_direction(bool dir);// 0：AB, 1：BA
  bool get_direction();

  // Device information
  uint8_t get_bootloader_version();
  uint8_t get_firmware_version();

 protected:
  bool read_register_(uint8_t reg_addr, uint8_t *data, uint8_t len);
  bool write_register_(uint8_t reg_addr, uint8_t *data, uint8_t len);

  // Initial configuration values
  bool initial_direction_{false};
  uint8_t initial_rgb_r_{0};
  uint8_t initial_rgb_g_{255};
  uint8_t initial_rgb_b_{0};
  
};

class UnitScrollSensor : public sensor::Sensor, public PollingComponent {
 public:
  void set_parent(UnitScrollComponent *parent) { this->parent_ = parent; }
  void update() override;
  void dump_config() override;

 protected:
  UnitScrollComponent *parent_{nullptr};
};

}  // namespace unit_scroll
}  // namespace esphome