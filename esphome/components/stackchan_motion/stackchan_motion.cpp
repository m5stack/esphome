#include "stackchan_motion.h"

#include <algorithm>

namespace esphome::stackchan_motion {

ESPHOME_LOG_TAG(TAG, "stackchan_motion");

static constexpr float DEG_TO_RAD = 0.017453292519943295f;
static constexpr float RAD_TO_DEG = 57.29577951308232f;
static constexpr float MIN_DIRECTION_LENGTH = 1e-6f;

void StackchanMotion::setup() {
  if (this->yaw_servo_ == nullptr || this->pitch_servo_ == nullptr) {
    this->mark_failed(LOG_STR("Both a yaw and a pitch servo are required"));
    return;
  }
  if (this->yaw_servo_ == this->pitch_servo_) {
    this->mark_failed(LOG_STR("The yaw and pitch servo must differ"));
    return;
  }

  if (this->min_yaw_ > this->max_yaw_) {
    std::swap(this->min_yaw_, this->max_yaw_);
  }
  if (this->min_pitch_ > this->max_pitch_) {
    std::swap(this->min_pitch_, this->max_pitch_);
  }
  this->home_yaw_ = std::clamp(this->home_yaw_, this->min_yaw_, this->max_yaw_);
  this->home_pitch_ = std::clamp(this->home_pitch_, this->min_pitch_, this->max_pitch_);
  this->target_yaw_ = this->home_yaw_;
  this->target_pitch_ = this->home_pitch_;

  this->batch_.add_servo(this->yaw_servo_);
  this->batch_.add_servo(this->pitch_servo_);
  if (!this->batch_.validate()) {
    this->mark_failed(LOG_STR("The servos do not belong to the same bus"));
    return;
  }

#if defined(USE_SENSOR) || defined(USE_NUMBER)
  this->yaw_servo_->add_on_feedback_callback([this]() { this->publish_feedback_(); });
  this->pitch_servo_->add_on_feedback_callback([this]() { this->publish_feedback_(); });
#endif
}

void StackchanMotion::dump_config() {
  ESP_LOGCONFIG(TAG, "Stackchan motion:");
  ESP_LOGCONFIG(TAG, "  Yaw range: %.1f to %.1f, pitch range: %.1f to %.1f", this->min_yaw_, this->max_yaw_,
                this->min_pitch_, this->max_pitch_);
  ESP_LOGCONFIG(TAG, "  Home: yaw %.1f, pitch %.1f, nudge step: %.1f", this->home_yaw_, this->home_pitch_,
                this->nudge_step_);
  ESP_LOGCONFIG(TAG, "  Default speed: %u steps/s, default time: %u ms", this->default_speed_, this->default_time_);
  ESP_LOGCONFIG(TAG, "  Target: yaw %.1f, pitch %.1f", this->target_yaw_, this->target_pitch_);
}

bool StackchanMotion::look_at(float yaw, float pitch) {
  return this->look_at(yaw, pitch, this->default_speed_, this->default_time_);
}

bool StackchanMotion::look_at(float yaw, float pitch, uint16_t speed, uint16_t time) {
  if (this->is_failed() || this->batch_.size() != 2) {
    return false;
  }

  yaw = std::clamp(yaw, this->min_yaw_, this->max_yaw_);
  pitch = std::clamp(pitch, this->min_pitch_, this->max_pitch_);

  // Both axes share one batch, so a transport with grouped writes starts them together.
  const float angles[2] = {yaw, pitch};
  if (!this->batch_.move_to_angles(angles, 2, speed, time)) {
    ESP_LOGW(TAG, "Failed to send yaw %.1f and pitch %.1f", yaw, pitch);
    return false;
  }

  this->target_yaw_ = yaw;
  this->target_pitch_ = pitch;
#ifdef USE_NUMBER
  // The pose numbers are controls: show the pose the head is heading for, and
  // let the idle feedback below replace it once the move is over.
  this->publish_pose_numbers_(yaw, pitch);
#endif
  return true;
}

bool StackchanMotion::look_direction(float x, float y, float z) {
  return this->look_direction(x, y, z, this->default_speed_, this->default_time_);
}

bool StackchanMotion::look_direction(float x, float y, float z, uint16_t speed, uint16_t time) {
  float yaw;
  float pitch;
  if (!StackchanMotion::to_angles(x, y, z, yaw, pitch)) {
    ESP_LOGW(TAG, "Cannot aim along a zero length direction");
    return false;
  }
  return this->look_at(yaw, pitch, speed, time);
}

bool StackchanMotion::nudge(float yaw_delta, float pitch_delta) {
  return this->look_at(this->target_yaw_ + yaw_delta, this->target_pitch_ + pitch_delta);
}

bool StackchanMotion::home() { return this->look_at(this->home_yaw_, this->home_pitch_); }

void StackchanMotion::stop() {
  if (this->yaw_servo_ != nullptr) {
    this->yaw_servo_->stop();
  }
  if (this->pitch_servo_ != nullptr) {
    this->pitch_servo_->stop();
  }
}

Direction StackchanMotion::to_direction(float yaw, float pitch) {
  const float yaw_rad = yaw * DEG_TO_RAD;
  const float cos_pitch = cosf(pitch * DEG_TO_RAD);
  return Direction{cos_pitch * sinf(yaw_rad), cos_pitch * cosf(yaw_rad), sinf(pitch * DEG_TO_RAD)};
}

bool StackchanMotion::to_angles(float x, float y, float z, float &yaw, float &pitch) {
  const float horizontal = sqrtf(x * x + y * y);
  if (horizontal < MIN_DIRECTION_LENGTH && fabsf(z) < MIN_DIRECTION_LENGTH) {
    return false;
  }
  yaw = atan2f(x, y) * RAD_TO_DEG;
  pitch = atan2f(z, horizontal) * RAD_TO_DEG;
  return true;
}

bool StackchanMotion::is_moving() const {
  return (this->yaw_servo_ != nullptr && this->yaw_servo_->is_moving()) ||
         (this->pitch_servo_ != nullptr && this->pitch_servo_->is_moving());
}

Direction StackchanMotion::direction() const {
  const float yaw = this->yaw_servo_ != nullptr ? this->yaw_servo_->current_angle() : NAN;
  const float pitch = this->pitch_servo_ != nullptr ? this->pitch_servo_->current_angle() : NAN;
  if (std::isnan(yaw) || std::isnan(pitch)) {
    return Direction{NAN, NAN, NAN};
  }
  return StackchanMotion::to_direction(yaw, pitch);
}

#ifdef USE_SENSOR
void StackchanMotion::publish_angle_sensor_(ftservo::FTServo *servo, sensor::Sensor *degrees) {
  if (servo == nullptr) {
    return;
  }
  PUBLISH_STATE(degrees, servo->current_angle());
}
#endif

#ifdef USE_NUMBER
void StackchanMotion::publish_pose_numbers_(float yaw, float pitch) {
  ftservo::publish_angle_state(this->yaw_number_, yaw, this->last_published_yaw_, false);
  ftservo::publish_angle_state(this->pitch_number_, pitch, this->last_published_pitch_, false);
}

void StackchanMotion::follow_axis_(ftservo::FTServo *servo, number::Number *number, float &target,
                                   float &last_published) {
  if (servo == nullptr || servo->is_moving()) {
    // A move owns the axis: the control keeps the pose it was commanded to.
    return;
  }
  const float measured = servo->current_angle();
  if (std::isnan(measured)) {
    return;
  }
  // A head that was pushed by hand is not where the last target says it is. While the axis is
  // idle the measurement is the pose, so relative moves and the control both start from it.
  target = measured;
  ftservo::publish_angle_state(number, measured, last_published, false);
}
#endif

void StackchanMotion::publish_feedback_() {
#ifdef USE_SENSOR
  this->publish_angle_sensor_(this->yaw_servo_, this->yaw_sensor_);
  this->publish_angle_sensor_(this->pitch_servo_, this->pitch_sensor_);

  const Direction dir = this->direction();
  PUBLISH_STATE(this->direction_x_sensor_, dir.x);
  PUBLISH_STATE(this->direction_y_sensor_, dir.y);
  PUBLISH_STATE(this->direction_z_sensor_, dir.z);
#endif
#ifdef USE_NUMBER
  this->follow_axis_(this->yaw_servo_, this->yaw_number_, this->target_yaw_, this->last_published_yaw_);
  this->follow_axis_(this->pitch_servo_, this->pitch_number_, this->target_pitch_, this->last_published_pitch_);
#endif
}

}  // namespace esphome::stackchan_motion
