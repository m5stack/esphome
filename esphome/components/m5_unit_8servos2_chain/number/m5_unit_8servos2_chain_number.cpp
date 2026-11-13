#include "m5_unit_8servos2_chain_number.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome::m5_unit_8servos2_chain {

ESPHOME_LOG_TAG(TAG, "m5_unit_8servos2_chain.number");

void M5Unit8Servos2AngleNumber::setup() {
  this->parent_->request_mode(this->channel_, Servos2Mode::SERVOS2_MODE_SERVO);
}

void M5Unit8Servos2AngleNumber::dump_config() {
  ESP_LOGCONFIG(TAG, "M5Stack Unit 8Servos2 Chain Servo Angle:");
  ESP_LOGCONFIG(TAG, "  Channel: %u", this->channel_);
  LOG_NUMBER(" ", "Servo Angle Number", this);
}

void M5Unit8Servos2AngleNumber::control(float value) {
  if (this->parent_ == nullptr)
    return;
  value = std::clamp(value, 0.0f, static_cast<float>(SERVOS2_SERVO_ANGLE_MAX));
  this->parent_->set_servo_angle_desired(this->channel_, static_cast<uint8_t>(value + 0.5f));
  this->publish_state(value);
}

void M5Unit8Servos2DutyNumber::setup() { this->parent_->request_mode(this->channel_, Servos2Mode::SERVOS2_MODE_PWM); }

void M5Unit8Servos2DutyNumber::dump_config() {
  ESP_LOGCONFIG(TAG, "M5Stack Unit 8Servos2 Chain PWM Duty:");
  ESP_LOGCONFIG(TAG, "  Channel: %u", this->channel_);
  LOG_NUMBER(" ", "PWM Duty Number", this);
}

void M5Unit8Servos2DutyNumber::control(float value) {
  if (this->parent_ == nullptr)
    return;
  value = std::clamp(value, 0.0f, static_cast<float>(SERVOS2_PWM_DUTY_MAX));
  this->parent_->set_pwm_duty_desired(this->channel_, static_cast<uint8_t>(value + 0.5f));
  this->publish_state(value);
}

}  // namespace esphome::m5_unit_8servos2_chain
