#pragma once

#include <cmath>

#include "esphome/components/ftservo/ftservo.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

namespace esphome::stackchan_motion {

/// A direction in the head frame: +X to the right, +Y forward, +Z up.
struct Direction {
  float x{0.0f};
  float y{1.0f};
  float z{0.0f};
};

/**
 * A two axis head: a yaw servo that turns around the vertical axis and a pitch servo that raises
 * and lowers the head. Both servo goals are written together, so the two axes start at the same
 * time with a shared move time.
 *
 * Angles are in degrees and measured from the servo calibration, so 0° on both axes points along
 * +Y. Yaw turns right for positive angles, pitch looks up.
 */
class StackchanMotion final : public Component {
 public:
  void setup() override;
  void dump_config() override;

  // Configuration, written by code generation.
  void set_yaw_servo(ftservo::FTServo *servo) { this->yaw_servo_ = servo; }
  void set_pitch_servo(ftservo::FTServo *servo) { this->pitch_servo_ = servo; }
  void set_min_yaw(float degrees) { this->min_yaw_ = degrees; }
  void set_max_yaw(float degrees) { this->max_yaw_ = degrees; }
  void set_min_pitch(float degrees) { this->min_pitch_ = degrees; }
  void set_max_pitch(float degrees) { this->max_pitch_ = degrees; }
  void set_home_yaw(float degrees) { this->home_yaw_ = degrees; }
  void set_home_pitch(float degrees) { this->home_pitch_ = degrees; }
  void set_nudge_step(float degrees) { this->nudge_step_ = degrees; }
  void set_default_speed(uint16_t speed) { this->default_speed_ = speed; }
  void set_default_time(uint16_t time) { this->default_time_ = time; }

  // Control, all non-blocking. A non-zero `time` makes both axes finish together.
  bool look_at(float yaw, float pitch);
  bool look_at(float yaw, float pitch, uint16_t speed, uint16_t time);
  bool look_direction(float x, float y, float z);
  bool look_direction(float x, float y, float z, uint16_t speed, uint16_t time);
  bool nudge(float yaw_delta, float pitch_delta);
  bool home();
  void stop();

  // Single axis entry points for the number entities.
  bool set_target_yaw(float yaw) { return this->look_at(yaw, this->target_pitch_); }
  bool set_target_pitch(float pitch) { return this->look_at(this->target_yaw_, pitch); }
  bool nudge_left() { return this->nudge(-this->nudge_step_, 0.0f); }
  bool nudge_right() { return this->nudge(this->nudge_step_, 0.0f); }
  bool nudge_up() { return this->nudge(0.0f, this->nudge_step_); }
  bool nudge_down() { return this->nudge(0.0f, -this->nudge_step_); }

  /// The look direction for a yaw/pitch pair.
  static Direction to_direction(float yaw, float pitch);
  /// The yaw and pitch that aim along (x, y, z); false when the vector has no length.
  static bool to_angles(float x, float y, float z, float &yaw, float &pitch);

  float target_yaw() const { return this->target_yaw_; }
  float target_pitch() const { return this->target_pitch_; }
  bool is_moving() const;
  /// The measured look direction; y is NAN when an axis has no feedback yet.
  Direction direction() const;

  // Optional entities, attached by the entity platforms.
#ifdef USE_SENSOR
  SUB_SENSOR(yaw)
  SUB_SENSOR(pitch)
  SUB_SENSOR(direction_x)
  SUB_SENSOR(direction_y)
  SUB_SENSOR(direction_z)
#endif
#ifdef USE_NUMBER
  SUB_NUMBER(yaw)
  SUB_NUMBER(pitch)
#endif
#ifdef USE_BUTTON
  SUB_BUTTON(home)
  SUB_BUTTON(left)
  SUB_BUTTON(right)
  SUB_BUTTON(up)
  SUB_BUTTON(down)
#endif

 protected:
#ifdef USE_SENSOR
  void publish_state_();
  void publish_angles_(ftservo::FTServo *servo, sensor::Sensor *degrees);
#endif

  ftservo::ServoBatch batch_{};
  ftservo::FTServo *yaw_servo_{nullptr};
  ftservo::FTServo *pitch_servo_{nullptr};

  float min_yaw_{-90.0f};
  float max_yaw_{90.0f};
  float min_pitch_{-45.0f};
  float max_pitch_{45.0f};
  float home_yaw_{0.0f};
  float home_pitch_{0.0f};
  float nudge_step_{10.0f};
  float target_yaw_{0.0f};
  float target_pitch_{0.0f};
  uint16_t default_speed_{0};
  uint16_t default_time_{0};
};

}  // namespace esphome::stackchan_motion
