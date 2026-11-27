#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensor/sensor_with_dedup.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

namespace esphome::ftservo {

/// Motion state of one servo. Errors are reported separately as a bit mask.
enum class ServoState : uint8_t {
  SERVO_STATE_IDLE,
  SERVO_STATE_MOVING,
  SERVO_STATE_HOLDING,
  SERVO_STATE_STALLED,
  SERVO_STATE_ERROR,
};

/// Communication and servo status errors, combined into one mask.
using ServoErrorMask = uint8_t;
constexpr ServoErrorMask SERVO_ERROR_NONE = 0;
constexpr ServoErrorMask SERVO_ERROR_NO_REPLY = 1 << 0;
constexpr ServoErrorMask SERVO_ERROR_CHECKSUM = 1 << 1;
constexpr ServoErrorMask SERVO_ERROR_ADDRESS = 1 << 2;
constexpr ServoErrorMask SERVO_ERROR_LENGTH = 1 << 3;
constexpr ServoErrorMask SERVO_ERROR_VOLTAGE = 1 << 4;
constexpr ServoErrorMask SERVO_ERROR_TEMPERATURE = 1 << 5;
constexpr ServoErrorMask SERVO_ERROR_OVERLOAD = 1 << 6;
constexpr ServoErrorMask SERVO_ERROR_STALLED = 1 << 7;

/// Feedback fields a servo model can report.
using ServoCapabilities = uint8_t;
constexpr ServoCapabilities SERVO_CAPABILITIES_POSITION = 1 << 0;
constexpr ServoCapabilities SERVO_CAPABILITIES_SPEED = 1 << 1;
constexpr ServoCapabilities SERVO_CAPABILITIES_LOAD = 1 << 2;
constexpr ServoCapabilities SERVO_CAPABILITIES_VOLTAGE = 1 << 3;
constexpr ServoCapabilities SERVO_CAPABILITIES_TEMPERATURE = 1 << 4;
constexpr ServoCapabilities SERVO_CAPABILITIES_CURRENT = 1 << 5;
constexpr ServoCapabilities SERVO_CAPABILITIES_MOVING = 1 << 6;

constexpr ServoCapabilities SERVO_CAPABILITIES_DEFAULT = SERVO_CAPABILITIES_POSITION | SERVO_CAPABILITIES_SPEED |
                                                         SERVO_CAPABILITIES_LOAD | SERVO_CAPABILITIES_VOLTAGE |
                                                         SERVO_CAPABILITIES_TEMPERATURE | SERVO_CAPABILITIES_MOVING;

/// One feedback sample, decoded by the model layer to the units noted here.
struct ServoFeedback {
  uint16_t position{0};   // steps
  int16_t speed{0};       // steps/s, negative when reversed
  int16_t load{0};        // 0.1 %, negative when reversed
  uint8_t voltage{0};     // 0.1 V
  int8_t temperature{0};  // °C
  int16_t current{0};     // model defined unit
  ServoErrorMask error{SERVO_ERROR_NONE};
  bool moving{false};
};

/**
 * One bus servo. This platform layer owns the model description, the unit conversion, the
 * motion state machine, the cached feedback and the entities. A concrete servo model only
 * implements the three transport hooks below.
 *
 * Transport contract: request_feedback_() is called at most once per update interval while no
 * earlier request is outstanding, and must later call either on_feedback() or
 * on_feedback_failed(). Returning false means the bus cannot take the request now; the platform
 * retries on the next loop. write_goal_() and write_torque_() return whether the command was
 * accepted; if the transport can tell later, it reports through on_command_result().
 */
class FTServo : public Component {
  friend class ServoBatch;

 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  // Configuration, written by code generation.
  void set_range_degrees(float range_degrees) { this->range_degrees_ = range_degrees; }
  void set_zero_position(uint16_t zero_position) { this->zero_position_ = zero_position; }
  void set_min_position(uint16_t min_position) { this->min_position_ = min_position; }
  void set_max_position(uint16_t max_position) { this->max_position_ = max_position; }
  void set_max_speed(uint16_t max_speed) { this->max_speed_ = max_speed; }
  void set_max_time(uint16_t max_time) { this->max_time_ = max_time; }
  void set_default_speed(uint16_t default_speed) { this->default_speed_ = default_speed; }
  void set_default_time(uint16_t default_time) { this->default_time_ = default_time; }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_release_after_move(bool release_after_move) { this->release_after_move_ = release_after_move; }
  void set_update_interval(uint32_t update_interval_ms) { this->update_interval_ms_ = update_interval_ms; }
  void set_capabilities(ServoCapabilities capabilities) { this->capabilities_ = capabilities; }
#ifdef USE_NUMBER
  void set_angle_follow_feedback(bool follow_feedback) { this->angle_follow_feedback_ = follow_feedback; }
#endif

  // Control, all non-blocking. A non-zero `time` takes precedence over `speed`.
  bool move_to(int position);
  bool move_to(int position, uint16_t speed, uint16_t time);
  bool move_to_angle(float degrees);
  bool move_to_angle(float degrees, uint16_t speed, uint16_t time);
  bool set_torque(bool enabled);
  void stop();

  // Called by the model layer when a transaction completes.
  void on_feedback(const ServoFeedback &feedback);
  void on_feedback_failed(ServoErrorMask error);
  void on_command_result(bool ok, ServoErrorMask error = SERVO_ERROR_NONE);

  ServoState state() const { return this->state_; }
  bool is_moving() const { return this->state_ == ServoState::SERVO_STATE_MOVING; }
  bool has_feedback() const { return this->feedback_valid_; }
  const ServoFeedback &feedback() const { return this->feedback_; }
  /// The measured angle in degrees, or NAN before the first feedback or without position feedback.
  float current_angle() const;

  template<typename F> void add_on_move_callback(F &&callback) { this->on_move_.add(std::forward<F>(callback)); }
  template<typename F> void add_on_move_done_callback(F &&callback) {
    this->on_move_done_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_feedback_callback(F &&callback) {
    this->on_feedback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_error_callback(F &&callback) { this->on_error_.add(std::forward<F>(callback)); }

  // Optional entities, attached by the entity platforms.
#ifdef USE_SENSOR
  SUB_SENSOR_WITH_DEDUP(position, uint16_t)
  SUB_SENSOR_WITH_DEDUP(speed, int16_t)
  SUB_SENSOR_WITH_DEDUP(load, float)
  SUB_SENSOR_WITH_DEDUP(voltage, float)
  SUB_SENSOR_WITH_DEDUP(temperature, int8_t)
  SUB_SENSOR_WITH_DEDUP(current, int16_t)
#endif
#ifdef USE_NUMBER
  SUB_NUMBER(position)
  SUB_NUMBER(angle)
#endif
#ifdef USE_SWITCH
  SUB_SWITCH(torque)
#endif

 protected:
  // Implemented by the concrete servo model.
  virtual bool write_goal_(uint16_t position, uint16_t speed, uint16_t time) = 0;
  virtual bool write_torque_(bool enabled) = 0;
  virtual bool request_feedback_() = 0;

  // Optional group support. A transport that can send several goals in one frame overrides these.
  // supports_group_write_() tells a group to use the batched path; begin_group_write_() returns
  // false when the transport is temporarily busy, in which case the group skips the update instead
  // of falling back to one write per servo.
  virtual bool supports_group_write_() const { return false; }
  virtual bool begin_group_write_() { return false; }
  virtual bool end_group_write_() { return false; }
  virtual void abort_group_write_() {}
  // Identifies the transport, so a group never mixes servos from two buses.
  virtual const void *transport_id_() const { return nullptr; }

  uint16_t clamp_position_(int position) const;
  float position_to_angle_(uint16_t position) const;
  uint16_t angle_to_position_(float degrees) const;
  uint32_t estimate_move_time_(uint16_t target, uint16_t speed, uint16_t time) const;
  void publish_feedback_();
  void complete_move_();
#ifdef USE_NUMBER
  void publish_angle_number_(float angle, bool force);
  void publish_feedback_angle_(uint16_t position);
#endif
  void update_errors_();

  LazyCallbackManager<void()> on_move_{};
  LazyCallbackManager<void()> on_move_done_{};
  LazyCallbackManager<void()> on_feedback_{};
  LazyCallbackManager<void(ServoErrorMask)> on_error_{};

  uint32_t update_interval_ms_{200};
  uint32_t last_poll_ms_{0};
  uint32_t request_started_ms_{0};
  uint32_t move_deadline_ms_{0};

  float range_degrees_{360.0f};
  ServoFeedback feedback_{};

  uint16_t zero_position_{0};
  uint16_t min_position_{0};
  uint16_t max_position_{1023};
  uint16_t max_speed_{1000};
  uint16_t max_time_{9999};
  uint16_t default_speed_{500};
  uint16_t default_time_{0};

  ServoState state_{ServoState::SERVO_STATE_IDLE};
  ServoCapabilities capabilities_{SERVO_CAPABILITIES_DEFAULT};
  ServoErrorMask comm_error_{SERVO_ERROR_NONE};
  ServoErrorMask status_error_{SERVO_ERROR_NONE};
  ServoErrorMask motion_error_{SERVO_ERROR_NONE};
  ServoErrorMask reported_errors_{SERVO_ERROR_NONE};
  bool feedback_valid_{false};
  bool awaiting_feedback_{false};
  bool torque_enabled_{false};
  bool inverted_{false};
  bool release_after_move_{false};
  bool restore_pending_{true};
#ifdef USE_NUMBER
  static constexpr size_t FEEDBACK_WINDOW_SIZE = 3;
  bool angle_follow_feedback_{true};
  float last_published_angle_{NAN};
  uint16_t feedback_window_[FEEDBACK_WINDOW_SIZE]{};
  uint8_t feedback_window_count_{0};
  uint8_t feedback_window_index_{0};
#endif
};

/// One target for a grouped move.
struct ServoGoal {
  uint16_t position{0};
  uint16_t speed{0};
  uint16_t time{0};
};

/**
 * Writes the goals of a fixed set of servos on one transport together. When the transport supports
 * grouped writes, all goals go out in one frame (scs9009 uses SYNC WRITE) so the servos start
 * together; otherwise they are written one after another.
 */
class ServoBatch {
 public:
  static constexpr size_t MAX_SERVOS = 8;

  bool add_servo(FTServo *servo);
  size_t size() const { return this->count_; }

  /// Whether the servos share a transport and the same group support. Logs the problem when not.
  bool validate() const;

  bool move_to(const ServoGoal *goals, size_t count);
  bool move_to_angles(const float *degrees, size_t count, uint16_t speed, uint16_t time);

 protected:
  template<typename F> bool move_with_(size_t count, F &&apply) {
    if (!this->servos_[0]->supports_group_write_()) {
      for (size_t i = 0; i < count; i++) {
        if (!apply(i)) {
          return false;
        }
      }
      return true;
    }
    if (!this->servos_[0]->begin_group_write_()) {
      return false;  // the transport is busy; the caller can retry on the next update
    }
    for (size_t i = 0; i < count; i++) {
      if (!apply(i)) {
        this->fail_batch_(i + 1);
        return false;
      }
    }
    if (this->servos_[0]->end_group_write_()) {
      return true;
    }
    this->fail_batch_(count);
    return false;
  }

  void fail_batch_(size_t count);

  std::array<FTServo *, MAX_SERVOS> servos_{};
  size_t count_{0};
};

/// A servo batch configured from YAML, for a component that commands the group as a whole.
class FTServoGroup final : public Component, public ServoBatch {
 public:
  void setup() override;
  void dump_config() override;

  /// The number of servos configured in YAML, checked against the ones added at startup.
  void set_expected_servos(uint8_t expected_servos) { this->expected_servos_ = expected_servos; }

 protected:
  uint8_t expected_servos_{0};
};

}  // namespace esphome::ftservo
