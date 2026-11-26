#include "ftservo.h"

#include "esphome/core/application.h"
#include "esphome/core/progmem.h"

namespace esphome::ftservo {

ESPHOME_LOG_TAG(TAG, "ftservo");

static constexpr uint16_t STEPS_PER_REVOLUTION = 1024;
static constexpr uint32_t MOVE_SLACK_MS = 500;
static constexpr uint32_t REQUEST_TIMEOUT_MS = 1000;
static constexpr float ANGLE_DEADBAND_DEGREES = 1.0f;

PROGMEM_STRING_TABLE(ServoStateString, "IDLE", "MOVING", "HOLDING", "STALLED", "ERROR");

void FTServo::setup() {
  if (this->range_degrees_ <= 0.0f) {
    this->range_degrees_ = 360.0f;
  }
  if (this->max_position_ <= this->min_position_) {
    this->max_position_ = this->min_position_;
  }
  if (this->default_speed_ == 0) {
    this->default_speed_ = 1;
  }
  this->default_speed_ = std::min(this->default_speed_, this->max_speed_);
  this->default_time_ = std::min(this->default_time_, this->max_time_);
  this->state_ = ServoState::SERVO_STATE_IDLE;
}

void FTServo::loop() {
  const uint32_t now = App.get_loop_component_start_time();

  if (this->restore_pending_) {
    this->restore_pending_ = false;
#ifdef USE_SWITCH
    if (this->torque_switch_ != nullptr) {
      this->torque_switch_->restore();
    }
#endif
  }

  if (this->awaiting_feedback_) {
    if (now - this->request_started_ms_ > REQUEST_TIMEOUT_MS) {
      this->on_feedback_failed(SERVO_ERROR_NO_REPLY);
    }
  } else if (now - this->last_poll_ms_ >= this->update_interval_ms_) {
    if (this->request_feedback_()) {
      this->last_poll_ms_ = now;
      this->request_started_ms_ = now;
      this->awaiting_feedback_ = true;
    }
  }

  if (this->state_ == ServoState::SERVO_STATE_MOVING && static_cast<int32_t>(now - this->move_deadline_ms_) >= 0) {
    this->state_ = ServoState::SERVO_STATE_STALLED;
    this->motion_error_ = SERVO_ERROR_STALLED;
    this->update_errors_();
  }
}

void FTServo::dump_config() {
  ESP_LOGCONFIG(TAG, "FTServo:");
  ESP_LOGCONFIG(TAG, "  Range: %.1f deg over %u steps, zero position: %u", this->range_degrees_, STEPS_PER_REVOLUTION,
                this->zero_position_);
  ESP_LOGCONFIG(TAG, "  Position limits: %u..%u, inverted: %s, release after move: %s", this->min_position_,
                this->max_position_, YESNO(this->inverted_), YESNO(this->release_after_move_));
  ESP_LOGCONFIG(TAG, "  Default speed: %u steps/s, default time: %u ms, update interval: %u ms", this->default_speed_,
                this->default_time_, static_cast<unsigned>(this->update_interval_ms_));
  ESP_LOGCONFIG(
      TAG, "  State: %s",
      LOG_STR_ARG(ServoStateString::get_log_str(static_cast<uint8_t>(this->state_), ServoStateString::LAST_INDEX)));
  if (this->feedback_valid_) {
    ESP_LOGCONFIG(TAG, "  Position: %u, speed: %d steps/s, load: %.1f %%, voltage: %.1f V, temperature: %d C",
                  this->feedback_.position, this->feedback_.speed, this->feedback_.load / 10.0f,
                  this->feedback_.voltage / 10.0f, this->feedback_.temperature);
  } else {
    ESP_LOGCONFIG(TAG, "  No feedback received yet");
  }
}

bool FTServo::move_to(int position) { return this->move_to(position, this->default_speed_, this->default_time_); }

bool FTServo::move_to(int position, uint16_t speed, uint16_t time) {
  const uint16_t target = this->clamp_position_(position);
  speed = std::min<uint16_t>(std::max<uint16_t>(speed == 0 ? this->default_speed_ : speed, 1), this->max_speed_);
  time = std::min<uint16_t>(time == 0 ? this->default_time_ : time, this->max_time_);

  if (!this->write_goal_(target, speed, time)) {
    ESP_LOGW(TAG, "Failed to send the goal position %u", target);
    return false;
  }

  const uint32_t now = App.get_loop_component_start_time();
  this->torque_enabled_ = true;  // writing a goal enables the torque output
  this->motion_error_ = SERVO_ERROR_NONE;
  this->state_ = ServoState::SERVO_STATE_MOVING;
  this->move_deadline_ms_ = now + this->estimate_move_time_(target, speed, time) + MOVE_SLACK_MS;
#ifdef USE_NUMBER
  // The angle number is a control, not a progress display: hold the commanded
  // angle until the move finishes instead of jumping to the old measured angle.
  this->publish_angle_number_(this->position_to_angle_(target), true);
#endif
  this->update_errors_();
  this->on_move_.call();
  return true;
}

bool FTServo::move_to_angle(float degrees) {
  return this->move_to_angle(degrees, this->default_speed_, this->default_time_);
}

bool FTServo::move_to_angle(float degrees, uint16_t speed, uint16_t time) {
  return this->move_to(this->angle_to_position_(degrees), speed, time);
}

bool FTServo::set_torque(bool enabled) {
  if (!this->write_torque_(enabled)) {
    ESP_LOGW(TAG, "Failed to %s the torque output", enabled ? LOG_STR_LITERAL("enable") : LOG_STR_LITERAL("disable"));
    return false;
  }
  const bool was_moving = this->state_ == ServoState::SERVO_STATE_MOVING;
  this->torque_enabled_ = enabled;
  if (enabled) {
    if (this->state_ == ServoState::SERVO_STATE_IDLE) {
      this->state_ = ServoState::SERVO_STATE_HOLDING;
    }
  } else {
    this->state_ = ServoState::SERVO_STATE_IDLE;
    this->motion_error_ = SERVO_ERROR_NONE;
    this->update_errors_();
    if (was_moving) {
      this->on_move_done_.call();
    }
  }
  return true;
}

void FTServo::stop() {
  if (!this->feedback_valid_) {
    ESP_LOGW(TAG, "Cannot stop before the first feedback read");
    return;
  }
  const bool was_moving = this->state_ == ServoState::SERVO_STATE_MOVING;
  if (!this->write_goal_(this->feedback_.position, 0, 0)) {
    ESP_LOGW(TAG, "Failed to send the stop command");
    return;
  }
  this->state_ = ServoState::SERVO_STATE_HOLDING;
  this->motion_error_ = SERVO_ERROR_NONE;
  this->update_errors_();
  if (was_moving) {
#ifdef USE_NUMBER
    // The move is over; snap the control to the measured value so a later
    // manual nudge starts from where the head actually is.
    this->publish_angle_number_(this->position_to_angle_(this->feedback_.position), true);
#endif
    this->on_move_done_.call();
  }
}

void FTServo::on_feedback(const ServoFeedback &feedback) {
  this->awaiting_feedback_ = false;
  this->feedback_ = feedback;
  this->feedback_valid_ = true;
  this->comm_error_ = SERVO_ERROR_NONE;
  this->status_error_ = feedback.error;
  this->publish_feedback_();
  this->on_feedback_.call();

  if (this->state_ == ServoState::SERVO_STATE_ERROR) {
    this->state_ = this->torque_enabled_ ? ServoState::SERVO_STATE_HOLDING : ServoState::SERVO_STATE_IDLE;
  }
  if (this->state_ == ServoState::SERVO_STATE_MOVING && (this->capabilities_ & SERVO_CAPABILITIES_MOVING) &&
      !feedback.moving) {
    this->complete_move_();
  }
  this->update_errors_();
}

void FTServo::on_feedback_failed(ServoErrorMask error) {
  this->awaiting_feedback_ = false;
  this->comm_error_ = error != SERVO_ERROR_NONE ? error : SERVO_ERROR_NO_REPLY;
  this->update_errors_();
}

void FTServo::on_command_result(bool ok, ServoErrorMask error) {
  this->comm_error_ = ok ? SERVO_ERROR_NONE : (error != SERVO_ERROR_NONE ? error : SERVO_ERROR_NO_REPLY);
  if (!ok && this->state_ == ServoState::SERVO_STATE_MOVING) {
    this->state_ = ServoState::SERVO_STATE_ERROR;  // the goal was never accepted by the servo
  }
  this->update_errors_();
}

float FTServo::current_angle() const {
  if (!this->feedback_valid_ || !(this->capabilities_ & SERVO_CAPABILITIES_POSITION)) {
    return NAN;
  }
  return this->position_to_angle_(this->feedback_.position);
}

uint16_t FTServo::clamp_position_(int position) const {
  if (position <= static_cast<int>(this->min_position_)) {
    return this->min_position_;
  }
  if (position >= static_cast<int>(this->max_position_)) {
    return this->max_position_;
  }
  return static_cast<uint16_t>(position);
}

float FTServo::position_to_angle_(uint16_t position) const {
  const float steps = static_cast<float>(position) - static_cast<float>(this->zero_position_);
  const float degrees = steps * this->range_degrees_ / STEPS_PER_REVOLUTION;
  return this->inverted_ ? -degrees : degrees;
}

uint16_t FTServo::angle_to_position_(float degrees) const {
  const float value = this->inverted_ ? -degrees : degrees;
  float position = static_cast<float>(this->zero_position_) + value * STEPS_PER_REVOLUTION / this->range_degrees_;
  position = std::clamp(position, static_cast<float>(this->min_position_), static_cast<float>(this->max_position_));
  return static_cast<uint16_t>(lroundf(position));
}

uint32_t FTServo::estimate_move_time_(uint16_t target, uint16_t speed, uint16_t time) const {
  if (time > 0) {
    return time;
  }
  uint32_t distance;
  if (this->feedback_valid_) {
    distance =
        target > this->feedback_.position ? target - this->feedback_.position : this->feedback_.position - target;
  } else {
    distance = this->max_position_ - this->min_position_;
  }
  return distance * 1000 / (speed > 0 ? speed : 1);
}

void FTServo::publish_feedback_() {
  const ServoFeedback &feedback = this->feedback_;
#ifdef USE_SENSOR
  if (this->capabilities_ & SERVO_CAPABILITIES_POSITION) {
    this->position_sensor_.publish_state_if_not_dup(feedback.position);
  } else {
    this->position_sensor_.publish_state_unknown();
  }
  if (this->capabilities_ & SERVO_CAPABILITIES_SPEED) {
    this->speed_sensor_.publish_state_if_not_dup(feedback.speed);
  } else {
    this->speed_sensor_.publish_state_unknown();
  }
  if (this->capabilities_ & SERVO_CAPABILITIES_LOAD) {
    this->load_sensor_.publish_state_if_not_dup(feedback.load / 10.0f);
  } else {
    this->load_sensor_.publish_state_unknown();
  }
  if (this->capabilities_ & SERVO_CAPABILITIES_VOLTAGE) {
    this->voltage_sensor_.publish_state_if_not_dup(feedback.voltage / 10.0f);
  } else {
    this->voltage_sensor_.publish_state_unknown();
  }
  if (this->capabilities_ & SERVO_CAPABILITIES_TEMPERATURE) {
    this->temperature_sensor_.publish_state_if_not_dup(feedback.temperature);
  } else {
    this->temperature_sensor_.publish_state_unknown();
  }
  if (this->capabilities_ & SERVO_CAPABILITIES_CURRENT) {
    this->current_sensor_.publish_state_if_not_dup(feedback.current);
  } else {
    this->current_sensor_.publish_state_unknown();
  }
#endif
#ifdef USE_NUMBER
  if (this->capabilities_ & SERVO_CAPABILITIES_POSITION) {
    PUBLISH_STATE(this->position_number_, static_cast<float>(feedback.position));
    // While moving, the angle number keeps the commanded target set by
    // move_to(); publishing the intermediate feedback makes the control chase
    // the servo and adds a large jump at the start and end of every move.
    if (this->state_ != ServoState::SERVO_STATE_MOVING) {
      this->publish_angle_number_(this->position_to_angle_(feedback.position), false);
    }
  } else {
    PUBLISH_STATE(this->position_number_, NAN);
    PUBLISH_STATE(this->angle_number_, NAN);
  }
#endif
}

#ifdef USE_NUMBER
void FTServo::publish_angle_number_(float angle, bool force) {
  if (this->angle_number_ == nullptr || std::isnan(angle)) {
    return;
  }
  if (!force && !std::isnan(this->last_published_angle_) &&
      std::fabs(angle - this->last_published_angle_) < ANGLE_DEADBAND_DEGREES) {
    return;
  }
  this->last_published_angle_ = angle;
  this->angle_number_->publish_state(std::round(angle));
}
#endif

void FTServo::complete_move_() {
  const bool was_moving = this->state_ == ServoState::SERVO_STATE_MOVING;
  this->motion_error_ = SERVO_ERROR_NONE;
  this->state_ = ServoState::SERVO_STATE_HOLDING;
  this->update_errors_();
  if (was_moving) {
#ifdef USE_NUMBER
    // The move is over; snap the control to the measured value so a later
    // manual nudge starts from where the head actually is.
    this->publish_angle_number_(this->position_to_angle_(this->feedback_.position), true);
#endif
    this->on_move_done_.call();
  }
  if (this->release_after_move_) {
    this->set_torque(false);
  }
}

void FTServo::update_errors_() {
  const ServoErrorMask effective = this->comm_error_ | this->status_error_ | this->motion_error_;
  if ((effective & ~this->reported_errors_) != 0) {
    if (this->comm_error_ != SERVO_ERROR_NONE) {
      this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    } else {
      this->status_set_warning(LOG_STR("Servo reported an error"));
    }
    this->on_error_.call(effective);
  } else if (effective == SERVO_ERROR_NONE) {
    this->status_clear_warning();
  }
  this->reported_errors_ = effective;
}

void FTServoGroup::setup() {
  if (this->count_ == 0 || this->count_ != this->expected_servos_) {
    this->mark_failed(LOG_STR("Servo list is incomplete"));
    return;
  }
  if (!this->validate()) {
    this->mark_failed(LOG_STR("Servos do not belong together"));
  }
}

void FTServoGroup::dump_config() {
  ESP_LOGCONFIG(TAG, "FTServo group:");
  ESP_LOGCONFIG(TAG, "  Servos: %u", static_cast<unsigned>(this->count_));
}

bool ServoBatch::add_servo(FTServo *servo) {
  if (servo == nullptr || this->count_ >= MAX_SERVOS) {
    ESP_LOGW(TAG, "Cannot add more than %u servos to one batch", static_cast<unsigned>(MAX_SERVOS));
    return false;
  }
  this->servos_[this->count_++] = servo;
  return true;
}

bool ServoBatch::validate() const {
  const void *transport = this->servos_[0]->transport_id_();
  const bool grouped = this->servos_[0]->supports_group_write_();
  for (size_t i = 1; i < this->count_; i++) {
    if (this->servos_[i]->transport_id_() != transport) {
      ESP_LOGE(TAG, "Servo %u is on a different bus", static_cast<unsigned>(i));
      return false;
    }
    if (this->servos_[i]->supports_group_write_() != grouped) {
      ESP_LOGE(TAG, "Servo %u has a different group support", static_cast<unsigned>(i));
      return false;
    }
  }
  return true;
}

bool ServoBatch::move_to(const ServoGoal *goals, size_t count) {
  if (goals == nullptr || count == 0 || count != this->count_) {
    return false;
  }
  return this->move_with_(count, [this, goals](size_t i) {
    return this->servos_[i]->move_to(goals[i].position, goals[i].speed, goals[i].time);
  });
}

bool ServoBatch::move_to_angles(const float *degrees, size_t count, uint16_t speed, uint16_t time) {
  if (degrees == nullptr || count == 0 || count != this->count_) {
    return false;
  }
  return this->move_with_(count, [this, degrees, speed, time](size_t i) {
    return this->servos_[i]->move_to_angle(degrees[i], speed, time);
  });
}

void ServoBatch::fail_batch_(size_t count) {
  this->servos_[0]->abort_group_write_();
  for (size_t i = 0; i < count; i++) {
    this->servos_[i]->on_command_result(false, SERVO_ERROR_LENGTH);
  }
}

}  // namespace esphome::ftservo
