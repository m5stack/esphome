#include <gtest/gtest.h>

#include "esphome/components/ftservo/ftservo.h"
#include "esphome/core/application.h"

namespace esphome::ftservo::testing {

namespace {

class TestServo : public FTServo {
 public:
  bool write_goal_(uint16_t position, uint16_t speed, uint16_t time) override {
    this->goals++;
    this->last_position = position;
    this->last_speed = speed;
    this->last_time = time;
    return this->goal_result;
  }
  bool write_torque_(bool enabled) override {
    this->torque_writes++;
    this->last_torque = enabled;
    return this->torque_result;
  }
  bool request_feedback_() override {
    this->feedback_requests++;
    return this->feedback_request_result;
  }
  bool supports_group_write_() const override { return this->group_supported; }
  bool begin_group_write_() override {
    this->group_begins++;
    return !this->group_busy;
  }
  bool end_group_write_() override {
    this->group_ends++;
    return !this->group_busy;
  }
  void abort_group_write_() override { this->group_aborts++; }
  const void *transport_id_() const override { return this->transport; }

  void configure() {
    this->set_range_degrees(300.0f);
    this->set_zero_position(500);
    this->set_min_position(0);
    this->set_max_position(1023);
    this->set_max_speed(1000);
    this->set_max_time(9999);
    this->set_default_speed(500);
    this->set_default_time(0);
    this->setup();
  }

  int goals{0};
  uint16_t last_position{0};
  uint16_t last_speed{0};
  uint16_t last_time{0};
  int torque_writes{0};
  bool last_torque{false};
  int feedback_requests{0};
  bool goal_result{true};
  bool torque_result{true};
  bool feedback_request_result{true};
  bool group_supported{false};
  bool group_busy{false};
  const void *transport{nullptr};
  int group_begins{0};
  int group_ends{0};
  int group_aborts{0};
};

}  // namespace

TEST(FTServoTest, ConvertsBetweenStepsAndDegrees) {
  TestServo servo;
  servo.configure();

  servo.move_to_angle(0.0f);
  EXPECT_EQ(servo.last_position, 500);

  servo.move_to_angle(90.0f);
  EXPECT_EQ(servo.last_position, 807);  // 500 + 90 * 1024 / 300

  servo.move_to_angle(-90.0f);
  EXPECT_EQ(servo.last_position, 193);

  servo.move_to_angle(1000.0f);  // clamped to the maximum position
  EXPECT_EQ(servo.last_position, 1023);
}

TEST(FTServoTest, InvertedServoMirrorsTheAngle) {
  TestServo servo;
  servo.configure();
  servo.set_inverted(true);

  servo.move_to_angle(90.0f);
  EXPECT_EQ(servo.last_position, 193);
}

TEST(FTServoTest, MoveIsNonBlockingAndCompletesOnFeedback) {
  TestServo servo;
  servo.configure();
  int moves = 0;
  int done = 0;
  servo.add_on_move_callback([&moves]() { moves++; });
  servo.add_on_move_done_callback([&done]() { done++; });

  ASSERT_TRUE(servo.move_to(600, 500, 0));
  EXPECT_EQ(servo.goals, 1);
  EXPECT_EQ(servo.last_position, 600);
  EXPECT_EQ(servo.last_speed, 500);
  EXPECT_EQ(servo.last_time, 0);
  EXPECT_EQ(servo.state(), ServoState::SERVO_STATE_MOVING);
  EXPECT_TRUE(servo.is_moving());
  EXPECT_EQ(moves, 1);
  EXPECT_EQ(done, 0);

  ServoFeedback feedback;
  feedback.position = 600;
  feedback.moving = false;
  servo.on_feedback(feedback);

  EXPECT_EQ(servo.state(), ServoState::SERVO_STATE_HOLDING);
  EXPECT_FALSE(servo.is_moving());
  EXPECT_EQ(done, 1);
  EXPECT_TRUE(servo.has_feedback());
  EXPECT_EQ(servo.feedback().position, 600);
}

TEST(FTServoTest, ReleasesTorqueAfterMoveWhenConfigured) {
  TestServo servo;
  servo.configure();
  servo.set_release_after_move(true);

  ASSERT_TRUE(servo.move_to(600));
  ServoFeedback feedback;
  feedback.position = 600;
  servo.on_feedback(feedback);

  EXPECT_EQ(servo.torque_writes, 1);
  EXPECT_FALSE(servo.last_torque);
  EXPECT_EQ(servo.state(), ServoState::SERVO_STATE_IDLE);
}

TEST(FTServoTest, ReportsStallWhenTheDeadlinePasses) {
  TestServo servo;
  servo.configure();
  uint8_t error = 0;
  servo.add_on_error_callback([&error](uint8_t code) { error = code; });

  ServoFeedback feedback;
  feedback.position = 500;
  servo.on_feedback(feedback);

  {
    LoopBlockingGuard clock{&servo, LOG_STR("test"), 1000};
    ASSERT_TRUE(servo.move_to(600, 100, 0));  // 100 steps at 100 steps/s, plus 500 ms slack
  }
  {
    LoopBlockingGuard clock{&servo, LOG_STR("test"), 1000 + 1501};
    servo.loop();
  }

  EXPECT_EQ(servo.state(), ServoState::SERVO_STATE_STALLED);
  EXPECT_EQ(error, SERVO_ERROR_STALLED);
}

TEST(FTServoTest, ReportsEachNewCommunicationErrorOnce) {
  TestServo servo;
  servo.configure();
  int errors = 0;
  uint8_t last_error = 0;
  servo.add_on_error_callback([&errors, &last_error](uint8_t code) {
    errors++;
    last_error = code;
  });

  servo.on_feedback_failed(SERVO_ERROR_CHECKSUM);
  EXPECT_EQ(errors, 1);
  EXPECT_EQ(last_error, SERVO_ERROR_CHECKSUM);

  servo.on_feedback_failed(SERVO_ERROR_CHECKSUM);
  EXPECT_EQ(errors, 1);

  ServoFeedback feedback;
  servo.on_feedback(feedback);  // success clears the error
  servo.on_feedback_failed(SERVO_ERROR_NO_REPLY);
  EXPECT_EQ(errors, 2);
  EXPECT_EQ(last_error, SERVO_ERROR_NO_REPLY);
}

TEST(FTServoTest, DisablingTorqueEndsARunningMove) {
  TestServo servo;
  servo.configure();
  int done = 0;
  servo.add_on_move_done_callback([&done]() { done++; });

  ASSERT_TRUE(servo.move_to(600));
  ASSERT_TRUE(servo.set_torque(false));

  EXPECT_EQ(servo.state(), ServoState::SERVO_STATE_IDLE);
  EXPECT_EQ(done, 1);
  EXPECT_FALSE(servo.is_moving());
}

TEST(FTServoTest, StopHoldsTheLastKnownPosition) {
  TestServo servo;
  servo.configure();

  ServoFeedback feedback;
  feedback.position = 321;
  servo.on_feedback(feedback);

  servo.stop();
  EXPECT_EQ(servo.last_position, 321);
  EXPECT_EQ(servo.last_speed, 0);
  EXPECT_EQ(servo.last_time, 0);
  EXPECT_EQ(servo.state(), ServoState::SERVO_STATE_HOLDING);
}

TEST(FTServoTest, MissedFeedbackDoesNotAbortAMove) {
  TestServo servo;
  servo.configure();

  ASSERT_TRUE(servo.move_to(600));
  servo.on_feedback_failed(SERVO_ERROR_NO_REPLY);
  EXPECT_EQ(servo.state(), ServoState::SERVO_STATE_MOVING);
}

TEST(FTServoTest, FailedGoalEntersErrorAndRecoversOnFeedback) {
  TestServo servo;
  servo.configure();

  ASSERT_TRUE(servo.move_to(600));
  servo.on_command_result(false, SERVO_ERROR_NO_REPLY);
  EXPECT_EQ(servo.state(), ServoState::SERVO_STATE_ERROR);

  ServoFeedback feedback;
  feedback.position = 590;
  servo.on_feedback(feedback);
  EXPECT_EQ(servo.state(), ServoState::SERVO_STATE_HOLDING);
}

TEST(FTServoTest, PollsFeedbackAtTheConfiguredInterval) {
  TestServo servo;
  servo.configure();
  servo.set_update_interval(200);

  {
    LoopBlockingGuard clock{&servo, LOG_STR("test"), 1000};
    servo.loop();  // first poll is due immediately
  }
  EXPECT_EQ(servo.feedback_requests, 1);

  {
    LoopBlockingGuard clock{&servo, LOG_STR("test"), 1100};
    servo.loop();  // a request is outstanding, so no second poll
  }
  EXPECT_EQ(servo.feedback_requests, 1);

  ServoFeedback feedback;
  servo.on_feedback(feedback);

  {
    LoopBlockingGuard clock{&servo, LOG_STR("test"), 1201};
    servo.loop();
  }
  EXPECT_EQ(servo.feedback_requests, 2);
}

TEST(FTServoGroupTest, SendsAllGoalsInOneBatch) {
  TestServo first;
  TestServo second;
  first.configure();
  second.configure();
  int transport;
  first.transport = &transport;
  second.transport = &transport;
  first.group_supported = true;
  second.group_supported = true;

  FTServoGroup group;
  group.set_expected_servos(2);
  ASSERT_TRUE(group.add_servo(&first));
  ASSERT_TRUE(group.add_servo(&second));

  const ServoGoal goals[2] = {{600, 500, 0}, {700, 600, 1000}};
  ASSERT_TRUE(group.move_to(goals, 2));

  EXPECT_EQ(first.group_begins, 1);
  EXPECT_EQ(first.group_ends, 1);
  EXPECT_EQ(second.group_begins, 0);
  EXPECT_EQ(second.group_ends, 0);
  EXPECT_EQ(first.goals, 1);
  EXPECT_EQ(second.goals, 1);
  EXPECT_EQ(first.last_position, 600);
  EXPECT_EQ(first.last_speed, 500);
  EXPECT_EQ(second.last_position, 700);
  EXPECT_EQ(second.last_speed, 600);
  EXPECT_EQ(second.last_time, 1000);
}

TEST(FTServoGroupTest, SendsAngleGoalsInOneBatch) {
  TestServo first;
  TestServo second;
  first.configure();
  second.configure();
  int transport;
  first.transport = &transport;
  second.transport = &transport;
  first.group_supported = true;
  second.group_supported = true;

  FTServoGroup group;
  group.set_expected_servos(2);
  ASSERT_TRUE(group.add_servo(&first));
  ASSERT_TRUE(group.add_servo(&second));

  const float angles[2] = {0.0f, 90.0f};
  ASSERT_TRUE(group.move_to_angles(angles, 2, 500, 1000));

  EXPECT_EQ(first.last_position, 500);
  EXPECT_EQ(second.last_position, 807);  // 500 + 90 * 1024 / 300
  EXPECT_EQ(second.last_time, 1000);
  EXPECT_EQ(first.group_ends, 1);
}

TEST(FTServoGroupTest, FallsBackToSequentialWrites) {
  TestServo first;
  TestServo second;
  first.configure();
  second.configure();
  int transport;
  first.transport = &transport;
  second.transport = &transport;

  FTServoGroup group;
  group.set_expected_servos(2);
  ASSERT_TRUE(group.add_servo(&first));
  ASSERT_TRUE(group.add_servo(&second));

  const ServoGoal goals[2] = {{600, 0, 0}, {700, 0, 0}};
  ASSERT_TRUE(group.move_to(goals, 2));

  EXPECT_EQ(first.group_begins, 0);  // the transport does not support batches
  EXPECT_EQ(first.goals, 1);
  EXPECT_EQ(second.goals, 1);
}

TEST(FTServoGroupTest, SkipsTheUpdateWhenTheTransportIsBusy) {
  TestServo first;
  TestServo second;
  first.configure();
  second.configure();
  int transport;
  first.transport = &transport;
  second.transport = &transport;
  first.group_supported = true;
  second.group_supported = true;
  first.group_busy = true;

  FTServoGroup group;
  group.set_expected_servos(2);
  ASSERT_TRUE(group.add_servo(&first));
  ASSERT_TRUE(group.add_servo(&second));

  const ServoGoal goals[2] = {{600, 0, 0}, {700, 0, 0}};
  EXPECT_FALSE(group.move_to(goals, 2));

  EXPECT_EQ(first.group_begins, 1);
  EXPECT_EQ(first.goals, 0);
  EXPECT_EQ(second.goals, 0);
}

TEST(FTServoGroupTest, AbortsABatchWhenAGoalIsRejected) {
  TestServo first;
  TestServo second;
  first.configure();
  second.configure();
  int transport;
  first.transport = &transport;
  second.transport = &transport;
  first.group_supported = true;
  second.group_supported = true;
  first.goal_result = false;

  FTServoGroup group;
  group.set_expected_servos(2);
  ASSERT_TRUE(group.add_servo(&first));
  ASSERT_TRUE(group.add_servo(&second));

  const ServoGoal goals[2] = {{600, 0, 0}, {700, 0, 0}};
  EXPECT_FALSE(group.move_to(goals, 2));

  EXPECT_EQ(first.group_aborts, 1);
  EXPECT_EQ(second.group_aborts, 0);
  EXPECT_EQ(first.goals, 1);
  EXPECT_EQ(second.goals, 0);
  EXPECT_EQ(first.state(), ServoState::SERVO_STATE_IDLE);
  EXPECT_TRUE(first.status_has_warning());
}

TEST(FTServoGroupTest, SetupAcceptsACompleteGroup) {
  TestServo first;
  TestServo second;
  int transport;
  first.transport = &transport;
  second.transport = &transport;
  first.group_supported = true;
  second.group_supported = true;

  FTServoGroup group;
  group.set_expected_servos(2);
  ASSERT_TRUE(group.add_servo(&first));
  ASSERT_TRUE(group.add_servo(&second));
  group.setup();

  EXPECT_FALSE(group.is_failed());
}

TEST(FTServoGroupTest, RejectsAnIncompleteGroup) {
  TestServo first;
  first.configure();

  FTServoGroup group;
  group.set_expected_servos(2);
  ASSERT_TRUE(group.add_servo(&first));
  group.setup();

  EXPECT_TRUE(group.is_failed());
}

TEST(FTServoGroupTest, RejectsServosFromDifferentTransports) {
  TestServo first;
  TestServo second;
  int transport_a;
  int transport_b;
  first.transport = &transport_a;
  second.transport = &transport_b;

  FTServoGroup group;
  group.set_expected_servos(2);
  ASSERT_TRUE(group.add_servo(&first));
  ASSERT_TRUE(group.add_servo(&second));
  group.setup();

  EXPECT_TRUE(group.is_failed());
}

TEST(FTServoGroupTest, RejectsMixedGroupSupport) {
  TestServo first;
  TestServo second;
  int transport;
  first.transport = &transport;
  second.transport = &transport;
  first.group_supported = true;

  FTServoGroup group;
  group.set_expected_servos(2);
  ASSERT_TRUE(group.add_servo(&first));
  ASSERT_TRUE(group.add_servo(&second));
  group.setup();

  EXPECT_TRUE(group.is_failed());
}

}  // namespace esphome::ftservo::testing
