#include "m5_unit_8servos2_chain_output.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome::m5_unit_8servos2_chain {

ESPHOME_LOG_TAG(TAG, "m5_unit_8servos2_chain.output");

void M5Unit8Servos2FloatOutput::setup() {
  this->parent_->request_mode(this->channel_, this->output_type_ == Servos2OutputType::SERVOS2_OUTPUT_TYPE_SERVO
                                                  ? Servos2Mode::SERVOS2_MODE_SERVO
                                                  : Servos2Mode::SERVOS2_MODE_PWM);
}

void M5Unit8Servos2FloatOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "M5Stack Unit 8Servos2 Chain Output:");
  ESP_LOGCONFIG(TAG, "  Channel: %u", this->channel_);
  ESP_LOGCONFIG(TAG, "  Type: %s",
                this->output_type_ == Servos2OutputType::SERVOS2_OUTPUT_TYPE_SERVO ? LOG_STR_LITERAL("servo")
                                                                                   : LOG_STR_LITERAL("pwm"));
  LOG_FLOAT_OUTPUT(this);
}

void M5Unit8Servos2FloatOutput::write_state(float state) {
  if (this->parent_ == nullptr)
    return;
  state = std::clamp(state, 0.0f, 1.0f);
  if (this->output_type_ == Servos2OutputType::SERVOS2_OUTPUT_TYPE_SERVO) {
    this->parent_->set_servo_angle_desired(this->channel_,
                                           static_cast<uint8_t>(state * SERVOS2_SERVO_ANGLE_MAX + 0.5f));
  } else {
    this->parent_->set_pwm_duty_desired(this->channel_, static_cast<uint8_t>(state * SERVOS2_PWM_DUTY_MAX + 0.5f));
  }
}

void M5Unit8Servos2BinaryOutput::setup() {
  this->parent_->request_mode(this->channel_, Servos2Mode::SERVOS2_MODE_OUTPUT);
}

void M5Unit8Servos2BinaryOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "M5Stack Unit 8Servos2 Chain Digital Output:");
  ESP_LOGCONFIG(TAG, "  Channel: %u", this->channel_);
  LOG_BINARY_OUTPUT(this);
}

void M5Unit8Servos2BinaryOutput::write_state(bool state) {
  if (this->parent_ != nullptr)
    this->parent_->set_output_level_desired(this->channel_, state);
}

}  // namespace esphome::m5_unit_8servos2_chain
