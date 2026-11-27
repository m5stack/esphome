#include <gtest/gtest.h>

#include <cmath>

#include "esphome/components/stackchan_motion/stackchan_motion.h"
#include "esphome/core/application.h"

namespace esphome::stackchan_motion::testing {

namespace {

class TestServo : public ftservo::FTServo {
 public:
  bool write_goal_(uint16_t position, uint16_t speed, uint16_t time) override {
    this->goals++;
    this->last_position = position;
    this->last_speed = speed;
    this->last_time = time;
    return this->goal_result;
  }
  bool write_torque_(bool enabled) override {
    this->last_torque = enabled;
    return true;
  }
  bool request_feedback_() override { return false; }

  bool supports_group_write_() const override { return this->group_supported; }
  bool begin_group_write_() override { return true; }
  bool end_group_write_() override { return true; }
  void abort_group_write_() override {}
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

  void report(float degrees) {
    ftservo::ServoFeedback feedback;
    feedback.position = static_cast<uint16_t>(lroundf(500.0f + degrees * 1024.0f / 300.0f));
    this->on_feedback(feedback);
  }

  int goals{0};
  uint16_t last_position{0};
  uint16_t last_speed{0};
  uint16_t last_time{0};
  bool last_torque{false};
  bool goal_result{true};
  bool group_supported{true};
  const void *transport{nullptr};
};

/// A head with two configured servos, ready for a move at clock 1000 ms.
struct Fixture {
  void init() {
    this->yaw.transport = &this->transport;
    this->pitch.transport = &this->transport;
    this->yaw.configure();
    this->pitch.configure();

    this->head.set_yaw_servo(&this->yaw);
    this->head.set_pitch_servo(&this->pitch);
    this->head.set_min_yaw(-60.0f);
    this->head.set_max_yaw(60.0f);
    this->head.set_min_pitch(-30.0f);
    this->head.set_max_pitch(30.0f);
    this->head.set_home_yaw(10.0f);
    this->head.set_home_pitch(-10.0f);
    this->head.set_nudge_step(5.0f);
  }

  int transport{0};
  TestServo yaw;
  TestServo pitch;
  StackchanMotion head;
};

}  // namespace

TEST(StackchanMotionTest, ConvertsAnglesToADirectionAndBack) {
  // Off the poles the conversion round-trips; at the pole the yaw is undefined.
  for (const auto &angles : {std::pair<float, float>{0.0f, 0.0f},
                             {30.0f, 0.0f},
                             {-45.0f, 20.0f},
                             {10.0f, -60.0f},
                             {170.0f, 5.0f},
                             {-120.0f, 80.0f}}) {
    const float yaw = angles.first;
    const float pitch = angles.second;
    const Direction dir = StackchanMotion::to_direction(yaw, pitch);
    EXPECT_NEAR(sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z), 1.0f, 1e-6f);

    float round_yaw;
    float round_pitch;
    ASSERT_TRUE(StackchanMotion::to_angles(dir.x, dir.y, dir.z, round_yaw, round_pitch));
    EXPECT_NEAR(round_pitch, pitch, 1e-4f);
    // yaw wraps at 180 deg, so compare it modulo a full turn
    EXPECT_NEAR(remainderf(round_yaw - yaw, 360.0f), 0.0f, 1e-4f);
  }
}

TEST(StackchanMotionTest, AimsStraightUpFromThePole) {
  float yaw = 123.0f;
  float pitch = 0.0f;
  ASSERT_TRUE(StackchanMotion::to_angles(0.0f, 0.0f, 1.0f, yaw, pitch));
  EXPECT_FLOAT_EQ(pitch, 90.0f);
  EXPECT_FLOAT_EQ(yaw, 0.0f);  // the yaw is undefined at the pole

  const Direction dir = StackchanMotion::to_direction(0.0f, 90.0f);
  EXPECT_NEAR(dir.z, 1.0f, 1e-6f);
}

TEST(StackchanMotionTest, RejectsAZeroLengthDirection) {
  float yaw = 0.0f;
  float pitch = 0.0f;
  EXPECT_FALSE(StackchanMotion::to_angles(0.0f, 0.0f, 0.0f, yaw, pitch));
}

TEST(StackchanMotionTest, MovesBothAxesWithOneSharedTime) {
  Fixture f;
  f.init();
  {
    LoopBlockingGuard clock{&f.head, LOG_STR("test"), 1000};
    f.head.setup();
    ASSERT_TRUE(f.head.look_at(20.0f, 15.0f, 400, 700));
  }

  EXPECT_EQ(f.yaw.goals, 1);
  EXPECT_EQ(f.pitch.goals, 1);
  EXPECT_EQ(f.yaw.last_position, 500 + 68);    // 20 deg * 1024 / 300
  EXPECT_EQ(f.pitch.last_position, 500 + 51);  // 15 deg * 1024 / 300
  EXPECT_EQ(f.yaw.last_time, 700);
  EXPECT_EQ(f.pitch.last_time, 700);
  EXPECT_EQ(f.yaw.last_speed, 400);
  EXPECT_EQ(f.pitch.last_speed, 400);
  EXPECT_FLOAT_EQ(f.head.target_yaw(), 20.0f);
  EXPECT_FLOAT_EQ(f.head.target_pitch(), 15.0f);
}

TEST(StackchanMotionTest, ClampsToTheConfiguredLimits) {
  Fixture f;
  f.init();
  {
    LoopBlockingGuard clock{&f.head, LOG_STR("test"), 1000};
    f.head.setup();
    ASSERT_TRUE(f.head.look_at(120.0f, -90.0f));
  }

  EXPECT_FLOAT_EQ(f.head.target_yaw(), 60.0f);
  EXPECT_FLOAT_EQ(f.head.target_pitch(), -30.0f);
}

TEST(StackchanMotionTest, NudgeAddsToTheCurrentTarget) {
  Fixture f;
  f.init();
  {
    LoopBlockingGuard clock{&f.head, LOG_STR("test"), 1000};
    f.head.setup();
    ASSERT_TRUE(f.head.look_at(20.0f, 10.0f));
    ASSERT_TRUE(f.head.nudge(5.0f, -5.0f));
    ASSERT_TRUE(f.head.nudge_right());
    ASSERT_TRUE(f.head.nudge_up());
  }

  EXPECT_FLOAT_EQ(f.head.target_yaw(), 20.0f + 5.0f + 5.0f);
  EXPECT_FLOAT_EQ(f.head.target_pitch(), 10.0f - 5.0f + 5.0f);
}

TEST(StackchanMotionTest, HomeReturnsToTheConfiguredPosition) {
  Fixture f;
  f.init();
  {
    LoopBlockingGuard clock{&f.head, LOG_STR("test"), 1000};
    f.head.setup();
    ASSERT_TRUE(f.head.look_at(40.0f, 20.0f));
    ASSERT_TRUE(f.head.home());
  }

  EXPECT_FLOAT_EQ(f.head.target_yaw(), 10.0f);
  EXPECT_FLOAT_EQ(f.head.target_pitch(), -10.0f);
}

TEST(StackchanMotionTest, LooksAlongADirection) {
  Fixture f;
  f.init();
  {
    LoopBlockingGuard clock{&f.head, LOG_STR("test"), 1000};
    f.head.setup();
    ASSERT_TRUE(f.head.look_direction(1.0f, 1.0f, 0.0f));  // 45 deg to the right, level
  }

  EXPECT_FLOAT_EQ(f.head.target_yaw(), 45.0f);
  EXPECT_FLOAT_EQ(f.head.target_pitch(), 0.0f);
}

TEST(StackchanMotionTest, SingleAxisTargetsKeepTheOtherAxis) {
  Fixture f;
  f.init();
  {
    LoopBlockingGuard clock{&f.head, LOG_STR("test"), 1000};
    f.head.setup();
    ASSERT_TRUE(f.head.set_target_yaw(25.0f));
  }
  EXPECT_FLOAT_EQ(f.head.target_yaw(), 25.0f);
  EXPECT_FLOAT_EQ(f.head.target_pitch(), -10.0f);  // the home position

  {
    LoopBlockingGuard clock{&f.head, LOG_STR("test"), 1001};
    ASSERT_TRUE(f.head.set_target_pitch(5.0f));
  }
  EXPECT_FLOAT_EQ(f.head.target_yaw(), 25.0f);
  EXPECT_FLOAT_EQ(f.head.target_pitch(), 5.0f);
}

TEST(StackchanMotionTest, IsMovingFollowsEitherAxis) {
  Fixture f;
  f.init();
  {
    LoopBlockingGuard clock{&f.head, LOG_STR("test"), 1000};
    f.head.setup();
    ASSERT_TRUE(f.head.look_at(30.0f, 0.0f));
  }
  EXPECT_TRUE(f.head.is_moving());

  f.yaw.report(0.0f);
  f.pitch.report(0.0f);
  EXPECT_FALSE(f.head.is_moving());
}

TEST(StackchanMotionTest, ReportsTheMeasuredDirection) {
  Fixture f;
  f.init();
  f.head.set_yaw_servo(&f.yaw);
  f.head.set_pitch_servo(&f.pitch);

  Direction dir = f.head.direction();
  EXPECT_TRUE(std::isnan(dir.y));

  f.yaw.report(0.0f);
  f.pitch.report(0.0f);
  dir = f.head.direction();
  EXPECT_NEAR(dir.x, 0.0f, 1e-5f);
  EXPECT_NEAR(dir.y, 1.0f, 1e-5f);
  EXPECT_NEAR(dir.z, 0.0f, 1e-5f);

  f.yaw.report(90.0f);
  f.pitch.report(0.0f);
  dir = f.head.direction();
  EXPECT_NEAR(dir.x, 1.0f, 5e-3f);  // one step covers 300/1024 deg
  EXPECT_NEAR(dir.y, 0.0f, 5e-3f);
}

TEST(StackchanMotionTest, FailsWithoutBothServos) {
  Fixture f;
  f.init();
  f.head.set_pitch_servo(nullptr);
  f.head.setup();
  EXPECT_TRUE(f.head.is_failed());

  StackchanMotion head;
  head.set_yaw_servo(&f.yaw);
  head.set_pitch_servo(&f.yaw);
  head.setup();
  EXPECT_TRUE(head.is_failed());
}

TEST(StackchanMotionTest, ClampsHomeIntoTheLimits) {
  Fixture f;
  f.init();
  f.head.set_home_yaw(200.0f);
  f.head.set_home_pitch(-200.0f);
  {
    LoopBlockingGuard clock{&f.head, LOG_STR("test"), 1000};
    f.head.setup();
  }

  EXPECT_FLOAT_EQ(f.head.target_yaw(), 60.0f);
  EXPECT_FLOAT_EQ(f.head.target_pitch(), -30.0f);
}

}  // namespace esphome::stackchan_motion::testing
