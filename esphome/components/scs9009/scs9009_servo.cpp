#include "scs9009_servo.h"

#include "esphome/core/log.h"

namespace esphome::scs9009 {

ESPHOME_LOG_TAG(TAG, "scs9009.servo");

void SCS9009Servo::setup() {
  ftservo::FTServo::setup();
  this->init_pending_ = true;
  if (!this->parent_->queue_read(this, this->address_, SCS_REG_ENDING, SCS_INIT_LENGTH, true)) {
    this->init_pending_ = false;
    this->status_set_warning(LOG_STR("Failed to queue the initialization read"));
  }
}

void SCS9009Servo::dump_config() {
  ftservo::FTServo::dump_config();
  ESP_LOGCONFIG(TAG, "  Address: %u, byte order: %s, write responses: %s", this->address_,
                this->big_endian_ ? LOG_STR_LITERAL("big endian") : LOG_STR_LITERAL("little endian"),
                YESNO(this->response_level_ != 0));
}

bool SCS9009Servo::write_goal_(uint16_t position, uint16_t speed, uint16_t time) {
  uint8_t data[6];
  scs_encode_word(&data[0], position, this->big_endian_);
  scs_encode_word(&data[2], time, this->big_endian_);
  scs_encode_word(&data[4], speed, this->big_endian_);
  if (this->parent_->sync_write_active()) {
    return this->parent_->add_sync_write(this, this->address_, SCS_REG_GOAL_POSITION, data, sizeof(data));
  }
  return this->parent_->queue_write(this, this->address_, SCS_REG_GOAL_POSITION, data, sizeof(data),
                                    this->response_level_ != 0);
}

bool SCS9009Servo::write_torque_(bool enabled) {
  const uint8_t value = enabled ? 1 : 0;
  return this->parent_->queue_write(this, this->address_, SCS_REG_TORQUE_ENABLE, &value, 1, this->response_level_ != 0);
}

bool SCS9009Servo::request_feedback_() {
  return this->parent_->queue_read(this, this->address_, SCS_REG_PRESENT_POSITION, SCS_FEEDBACK_LENGTH, true);
}

bool SCS9009Servo::begin_group_write_() { return this->parent_->begin_sync_write(); }

bool SCS9009Servo::end_group_write_() { return this->parent_->end_sync_write(); }

void SCS9009Servo::abort_group_write_() { this->parent_->abort_sync_write(); }

const void *SCS9009Servo::transport_id_() const { return this->parent_; }

void SCS9009Servo::on_scs_read(uint8_t reg, const uint8_t *data, uint8_t length, uint8_t status) {
  if (this->init_pending_ && reg == SCS_REG_ENDING) {
    this->init_pending_ = false;
    if (length < SCS_INIT_LENGTH) {
      ESP_LOGW(TAG, "Servo %u returned a short initialization read", this->address_);
      this->on_feedback_failed(ftservo::SERVO_ERROR_LENGTH);
      return;
    }
    this->big_endian_ = data[0] != 0;
    this->response_level_ = data[SCS_REG_RESPONSE_LEVEL - SCS_REG_ENDING];
    ESP_LOGD(TAG, "Servo %u: firmware %u.%u, big endian: %s, response level: %u", this->address_, data[1], data[2],
             this->big_endian_ ? LOG_STR_LITERAL("yes") : LOG_STR_LITERAL("no"), this->response_level_);
    return;
  }

  if (reg != SCS_REG_PRESENT_POSITION || length < SCS_FEEDBACK_LENGTH) {
    ESP_LOGW(TAG, "Unexpected read from servo %u (register %u, length %u)", this->address_, reg, length);
    this->on_feedback_failed(ftservo::SERVO_ERROR_LENGTH);
    return;
  }

  ftservo::ServoFeedback feedback;
  feedback.position = scs_decode_word(&data[0], this->big_endian_);
  feedback.speed = scs_decode_signed(
      scs_decode_word(&data[SCS_REG_PRESENT_SPEED - SCS_REG_PRESENT_POSITION], this->big_endian_), 15);
  feedback.load =
      scs_decode_signed(scs_decode_word(&data[SCS_REG_PRESENT_LOAD - SCS_REG_PRESENT_POSITION], this->big_endian_), 10);
  feedback.voltage = data[SCS_REG_PRESENT_VOLTAGE - SCS_REG_PRESENT_POSITION];
  feedback.temperature = static_cast<int8_t>(data[SCS_REG_PRESENT_TEMPERATURE - SCS_REG_PRESENT_POSITION]);
  feedback.moving = data[SCS_REG_PRESENT_MOVING - SCS_REG_PRESENT_POSITION] != 0;

  feedback.error = ftservo::SERVO_ERROR_NONE;
  const uint8_t status_bits = static_cast<uint8_t>(status | data[SCS_REG_STATUS - SCS_REG_PRESENT_POSITION]);
  if (status_bits & SCS_STATUS_VOLTAGE) {
    feedback.error |= ftservo::SERVO_ERROR_VOLTAGE;
  }
  if (status_bits & SCS_STATUS_TEMPERATURE) {
    feedback.error |= ftservo::SERVO_ERROR_TEMPERATURE;
  }
  if (status_bits & SCS_STATUS_OVERLOAD) {
    feedback.error |= ftservo::SERVO_ERROR_OVERLOAD;
  }

  this->on_feedback(feedback);
}

void SCS9009Servo::on_scs_read_failed(ftservo::ServoErrorMask error) {
  if (this->init_pending_) {
    this->init_pending_ = false;
    ESP_LOGW(TAG, "Servo %u did not answer the initialization read", this->address_);
  }
  this->on_feedback_failed(error);
}

void SCS9009Servo::on_scs_write(bool ok, ftservo::ServoErrorMask error) { this->on_command_result(ok, error); }

}  // namespace esphome::scs9009
