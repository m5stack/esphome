#include "m5_voice_pyramid_sensor.h"

#include "esphome/core/log.h"

namespace esphome::m5_voice_pyramid_kit {

static const char *const TAG = "m5_voice_pyramid_kit.sensor";

void M5VoicePyramidKitSensor::setup() {
  switch (this->type_) {
    case SWIPE_EVENT:
      this->parent_->add_swipe_callback([this](uint8_t code) { this->publish_state(static_cast<float>(code)); });
      break;
    case USB_VOLTAGE:
      this->parent_->add_voltage_callback([this](float voltage_mv) { this->publish_state(voltage_mv); });
      break;
  }
}

void M5VoicePyramidKitSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "M5Stack Voice Pyramid Kit %s", this->type_ == SWIPE_EVENT ? "Swipe Event" : "USB Voltage");
  LOG_SENSOR("  ", "Sensor", this);
}

}  // namespace esphome::m5_voice_pyramid_kit
