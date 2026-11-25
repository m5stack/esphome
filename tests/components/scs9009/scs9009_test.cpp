#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/scs9009/scs9009_servo.h"
#include "esphome/components/scs9009/scs_frame.h"

namespace esphome::scs9009::testing {

namespace {

uint8_t checksum(uint8_t address, uint8_t length, uint8_t status, const std::vector<uint8_t> &data) {
  uint8_t sum = address + length + status;
  for (uint8_t byte : data) {
    sum = static_cast<uint8_t>(sum + byte);
  }
  return static_cast<uint8_t>(~sum);
}

std::vector<uint8_t> make_response(uint8_t address, uint8_t status, const std::vector<uint8_t> &data) {
  std::vector<uint8_t> frame = {SCS_HEADER, SCS_HEADER, address, static_cast<uint8_t>(data.size() + 2), status};
  frame.insert(frame.end(), data.begin(), data.end());
  frame.push_back(checksum(address, static_cast<uint8_t>(data.size() + 2), status, data));
  return frame;
}

void feed_all(SCSFrameParser &parser, const std::vector<uint8_t> &bytes) {
  for (uint8_t byte : bytes) {
    parser.feed(byte);
  }
}

}  // namespace

TEST(SCSFrameTest, BuildsReadFrame) {
  uint8_t buffer[SCS_MAX_FRAME_SIZE];
  const uint8_t params[] = {SCS_REG_PRESENT_POSITION, SCS_FEEDBACK_LENGTH};
  const size_t length = scs_build_frame(buffer, sizeof(buffer), 1, SCS_INSTRUCTION_READ, params, sizeof(params));

  ASSERT_EQ(length, 8);
  EXPECT_EQ(buffer[0], 0xFF);
  EXPECT_EQ(buffer[1], 0xFF);
  EXPECT_EQ(buffer[2], 0x01);
  EXPECT_EQ(buffer[3], 0x04);  // two parameters plus two
  EXPECT_EQ(buffer[4], SCS_INSTRUCTION_READ);
  EXPECT_EQ(buffer[5], 56);
  EXPECT_EQ(buffer[6], 11);
  EXPECT_EQ(buffer[7], checksum(1, 4, SCS_INSTRUCTION_READ, {56, 11}));
}

TEST(SCSFrameTest, BuildsWriteFrame) {
  uint8_t buffer[SCS_MAX_FRAME_SIZE];
  const uint8_t params[] = {SCS_REG_GOAL_POSITION, 0x02, 0x00, 0x00, 0x00, 0x01, 0xF4};
  const size_t length = scs_build_frame(buffer, sizeof(buffer), 2, SCS_INSTRUCTION_WRITE, params, sizeof(params));

  ASSERT_EQ(length, 13);
  EXPECT_EQ(buffer[2], 0x02);
  EXPECT_EQ(buffer[3], 0x09);
  EXPECT_EQ(buffer[4], SCS_INSTRUCTION_WRITE);
  EXPECT_EQ(buffer[12],
            checksum(2, 9, SCS_INSTRUCTION_WRITE, {SCS_REG_GOAL_POSITION, 0x02, 0x00, 0x00, 0x00, 0x01, 0xF4}));
}

TEST(SCSFrameTest, BuildsSyncWriteFrameFromTheManualExample) {
  uint8_t buffer[SCS_MAX_FRAME_SIZE];
  const uint8_t data[6] = {0x00, 0x08, 0x00, 0x00, 0xE8, 0x03};
  const SCSSyncWriteEntry entries[4] = {{1, data}, {2, data}, {3, data}, {4, data}};
  const size_t length = scs_build_sync_write(buffer, sizeof(buffer), SCS_REG_GOAL_POSITION, sizeof(data), entries, 4);

  const uint8_t expected[36] = {0xFF, 0xFF, 0xFE, 0x20, 0x83, 0x2A, 0x06, 0x01, 0x00, 0x08, 0x00, 0x00,
                                0xE8, 0x03, 0x02, 0x00, 0x08, 0x00, 0x00, 0xE8, 0x03, 0x03, 0x00, 0x08,
                                0x00, 0x00, 0xE8, 0x03, 0x04, 0x00, 0x08, 0x00, 0x00, 0xE8, 0x03, 0x58};
  ASSERT_EQ(length, sizeof(expected));
  EXPECT_EQ(std::vector<uint8_t>(buffer, buffer + length), std::vector<uint8_t>(expected, expected + sizeof(expected)));
}

TEST(SCSFrameTest, BuildsSyncReadFrameFromTheManualExample) {
  uint8_t buffer[SCS_MAX_FRAME_SIZE];
  const uint8_t addresses[2] = {1, 2};
  const size_t length = scs_build_sync_read(buffer, sizeof(buffer), SCS_REG_PRESENT_POSITION, 8, addresses, 2);

  const uint8_t expected[10] = {0xFF, 0xFF, 0xFE, 0x06, 0x82, 0x38, 0x08, 0x01, 0x02, 0x36};
  ASSERT_EQ(length, sizeof(expected));
  EXPECT_EQ(std::vector<uint8_t>(buffer, buffer + length), std::vector<uint8_t>(expected, expected + sizeof(expected)));
}

TEST(SCSFrameTest, RejectsInvalidSyncFrames) {
  uint8_t buffer[SCS_MAX_FRAME_SIZE];
  const uint8_t data[6] = {};
  const SCSSyncWriteEntry entries[1] = {{1, data}};
  const uint8_t addresses[1] = {1};

  EXPECT_EQ(scs_build_sync_write(buffer, sizeof(buffer), SCS_REG_GOAL_POSITION, 0, entries, 1), 0);
  EXPECT_EQ(scs_build_sync_write(buffer, sizeof(buffer), SCS_REG_GOAL_POSITION, sizeof(data), entries, 0), 0);
  EXPECT_EQ(scs_build_sync_write(buffer, sizeof(buffer), SCS_REG_GOAL_POSITION, SCS_MAX_SYNC_DATA + 1, entries, 1), 0);

  const SCSSyncWriteEntry too_many[SCS_MAX_SYNC_SERVOS + 1] = {};
  EXPECT_EQ(scs_build_sync_write(buffer, sizeof(buffer), SCS_REG_GOAL_POSITION, sizeof(data), too_many,
                                 SCS_MAX_SYNC_SERVOS + 1),
            0);

  EXPECT_EQ(scs_build_sync_read(buffer, sizeof(buffer), SCS_REG_PRESENT_POSITION, 8, addresses, 0), 0);
  EXPECT_EQ(scs_build_sync_read(buffer, sizeof(buffer), SCS_REG_PRESENT_POSITION, 0, addresses, 1), 0);
  EXPECT_EQ(scs_build_sync_read(buffer, sizeof(buffer), SCS_REG_PRESENT_POSITION, SCS_MAX_PAYLOAD + 1, addresses, 1),
            0);
}

TEST(SCSFrameTest, RefusesTooSmallBuffer) {
  uint8_t buffer[4];
  const uint8_t params[] = {SCS_REG_PRESENT_POSITION, SCS_FEEDBACK_LENGTH};
  EXPECT_EQ(scs_build_frame(buffer, sizeof(buffer), 1, SCS_INSTRUCTION_READ, params, sizeof(params)), 0);
}

TEST(SCSFrameTest, ParsesPingResponse) {
  SCSFrameParser parser;
  const std::vector<uint8_t> frame = {0xFF, 0xFF, 0x01, 0x02, 0x00, 0xFC};

  SCSFrameResult result = SCSFrameResult::SCS_FRAME_RESULT_IN_PROGRESS;
  for (uint8_t byte : frame) {
    result = parser.feed(byte);
  }

  ASSERT_EQ(result, SCSFrameResult::SCS_FRAME_RESULT_COMPLETE);
  EXPECT_EQ(parser.frame().address, 1);
  EXPECT_EQ(parser.frame().status, 0);
  EXPECT_EQ(parser.frame().data_len, 0);
  EXPECT_EQ(parser.raw_length(), frame.size());
}

TEST(SCSFrameTest, ParsesFeedbackResponse) {
  const std::vector<uint8_t> data = {0x02, 0x00, 0x00, 0x64, 0x04, 0x0A, 74, 35, 0, 0, 1};
  const std::vector<uint8_t> frame = make_response(1, 0, data);

  SCSFrameParser parser;
  SCSFrameResult result = SCSFrameResult::SCS_FRAME_RESULT_IN_PROGRESS;
  for (uint8_t byte : frame) {
    result = parser.feed(byte);
  }

  ASSERT_EQ(result, SCSFrameResult::SCS_FRAME_RESULT_COMPLETE);
  ASSERT_EQ(parser.frame().data_len, data.size());
  EXPECT_EQ(std::vector<uint8_t>(parser.frame().data, parser.frame().data + parser.frame().data_len), data);
}

TEST(SCSFrameTest, SkipsNoiseBeforeTheHeader) {
  SCSFrameParser parser;
  const std::vector<uint8_t> noise = {0x00, 0xFF, 0x00, 0x12, 0xFF};
  feed_all(parser, noise);

  const std::vector<uint8_t> frame = make_response(3, 0, {0x01});
  feed_all(parser, frame);

  ASSERT_EQ(parser.frame().address, 3);
  EXPECT_EQ(parser.frame().data_len, 1);
  EXPECT_EQ(parser.frame().data[0], 1);
}

TEST(SCSFrameTest, RejectsBadChecksum) {
  SCSFrameParser parser;
  std::vector<uint8_t> frame = make_response(1, 0, {0x01, 0x02});
  frame.back() ^= 0xFF;

  SCSFrameResult result = SCSFrameResult::SCS_FRAME_RESULT_IN_PROGRESS;
  for (uint8_t byte : frame) {
    result = parser.feed(byte);
  }
  EXPECT_EQ(result, SCSFrameResult::SCS_FRAME_RESULT_INVALID);
}

TEST(SCSFrameTest, RejectsInvalidLengths) {
  SCSFrameParser parser;
  EXPECT_EQ(parser.feed(0xFF), SCSFrameResult::SCS_FRAME_RESULT_IN_PROGRESS);
  EXPECT_EQ(parser.feed(0xFF), SCSFrameResult::SCS_FRAME_RESULT_IN_PROGRESS);
  EXPECT_EQ(parser.feed(0x01), SCSFrameResult::SCS_FRAME_RESULT_IN_PROGRESS);
  EXPECT_EQ(parser.feed(0x00), SCSFrameResult::SCS_FRAME_RESULT_INVALID);  // length below two

  parser.reset();
  EXPECT_EQ(parser.feed(0xFF), SCSFrameResult::SCS_FRAME_RESULT_IN_PROGRESS);
  EXPECT_EQ(parser.feed(0xFF), SCSFrameResult::SCS_FRAME_RESULT_IN_PROGRESS);
  EXPECT_EQ(parser.feed(0x01), SCSFrameResult::SCS_FRAME_RESULT_IN_PROGRESS);
  EXPECT_EQ(parser.feed(0xFF), SCSFrameResult::SCS_FRAME_RESULT_INVALID);  // frame longer than the buffer
}

TEST(SCSFrameTest, DecodesWordsAndSigns) {
  uint8_t buffer[2];
  scs_encode_word(buffer, 0x1234, true);
  EXPECT_EQ(buffer[0], 0x12);
  EXPECT_EQ(buffer[1], 0x34);
  EXPECT_EQ(scs_decode_word(buffer, true), 0x1234);

  scs_encode_word(buffer, 0x1234, false);
  EXPECT_EQ(buffer[0], 0x34);
  EXPECT_EQ(buffer[1], 0x12);
  EXPECT_EQ(scs_decode_word(buffer, false), 0x1234);

  EXPECT_EQ(scs_decode_signed(0x0064, 15), 100);
  EXPECT_EQ(scs_decode_signed(0x8000 | 0x0064, 15), -100);
  EXPECT_EQ(scs_decode_signed(0x000A, 10), 10);
  EXPECT_EQ(scs_decode_signed(0x040A, 10), -10);
}

TEST(SCSFrameTest, ServoDecodesFeedback) {
  SCS9009Servo servo;
  int errors = 0;
  uint8_t last_error = 0;
  servo.add_on_error_callback([&errors, &last_error](uint8_t code) {
    errors++;
    last_error = code;
  });

  const uint8_t data[SCS_FEEDBACK_LENGTH] = {0x02, 0x00, 0x00, 0x64, 0x04, 0x0A, 74, 35, 0, 0, 1};
  servo.on_scs_read(SCS_REG_PRESENT_POSITION, data, sizeof(data), SCS_STATUS_VOLTAGE);

  EXPECT_TRUE(servo.has_feedback());
  EXPECT_EQ(servo.feedback().position, 512);
  EXPECT_EQ(servo.feedback().speed, 100);
  EXPECT_EQ(servo.feedback().load, -10);
  EXPECT_EQ(servo.feedback().voltage, 74);
  EXPECT_EQ(servo.feedback().temperature, 35);
  EXPECT_TRUE(servo.feedback().moving);
  EXPECT_EQ(servo.feedback().error, ftservo::SERVO_ERROR_VOLTAGE);
  EXPECT_EQ(errors, 1);
  EXPECT_EQ(last_error, ftservo::SERVO_ERROR_VOLTAGE);
}

TEST(SCSFrameTest, CombinesTheResponseAndStatusRegisterBits) {
  SCS9009Servo servo;
  const uint8_t data[SCS_FEEDBACK_LENGTH] = {0x02, 0x00, 0x00, 0x64, 0x04, 0x0A, 74, 35, 0, SCS_STATUS_TEMPERATURE, 0};
  servo.on_scs_read(SCS_REG_PRESENT_POSITION, data, sizeof(data), 0);

  EXPECT_TRUE(servo.has_feedback());
  EXPECT_EQ(servo.feedback().error, ftservo::SERVO_ERROR_TEMPERATURE);
}

}  // namespace esphome::scs9009::testing
