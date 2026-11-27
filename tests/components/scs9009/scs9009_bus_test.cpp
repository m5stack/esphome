#include <deque>
#include <gtest/gtest.h>
#include <vector>

#include "esphome/components/scs9009/scs9009.h"
#include "esphome/components/scs9009/scs9009_servo.h"
#include "esphome/core/application.h"

namespace esphome::scs9009::testing {

namespace {

class TestUART : public uart::UARTComponent {
 public:
  void write_array(const uint8_t *data, size_t len) override { this->tx.insert(this->tx.end(), data, data + len); }
  bool peek_byte(uint8_t *data) override {
    if (this->rx.empty()) {
      return false;
    }
    *data = this->rx.front();
    return true;
  }
  bool read_array(uint8_t *data, size_t len) override {
    if (this->rx.size() < len) {
      return false;
    }
    for (size_t i = 0; i < len; i++) {
      data[i] = this->rx.front();
      this->rx.pop_front();
    }
    return true;
  }
  size_t available() override { return this->rx.size(); }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }
  void check_logger_conflict() override {}

  std::deque<uint8_t> rx;
  std::vector<uint8_t> tx;
};

std::vector<uint8_t> feedback_response(uint8_t address, uint8_t status, uint16_t position = 500) {
  const std::vector<uint8_t> data = {static_cast<uint8_t>(position >> 8),
                                     static_cast<uint8_t>(position & 0xFF),
                                     0x00,
                                     0x64,
                                     0x04,
                                     0x0A,
                                     74,
                                     35,
                                     0,
                                     0,
                                     0};
  std::vector<uint8_t> frame = {SCS_HEADER, SCS_HEADER, address, static_cast<uint8_t>(data.size() + 2), status};
  frame.insert(frame.end(), data.begin(), data.end());
  uint8_t sum = address + static_cast<uint8_t>(data.size() + 2) + status;
  for (uint8_t byte : data) {
    sum = static_cast<uint8_t>(sum + byte);
  }
  frame.push_back(static_cast<uint8_t>(~sum));
  return frame;
}

void feed(TestUART &uart, const std::vector<uint8_t> &bytes) {
  uart.rx.insert(uart.rx.end(), bytes.begin(), bytes.end());
}

}  // namespace

TEST(SCS9009BusTest, SendsAReadRequest) {
  TestUART uart;
  SCS9009Component bus;
  bus.set_uart_parent(&uart);
  SCS9009Servo servo;
  servo.set_address(1);
  servo.set_parent(&bus);

  {
    LoopBlockingGuard clock{&bus, LOG_STR("test"), 1000};
    ASSERT_TRUE(bus.queue_read(&servo, 1, SCS_REG_PRESENT_POSITION, SCS_FEEDBACK_LENGTH, true));
    bus.loop();
  }

  ASSERT_EQ(uart.tx.size(), 8);
  EXPECT_EQ(uart.tx[0], 0xFF);
  EXPECT_EQ(uart.tx[1], 0xFF);
  EXPECT_EQ(uart.tx[2], 1);
  EXPECT_EQ(uart.tx[3], 4);
  EXPECT_EQ(uart.tx[4], SCS_INSTRUCTION_READ);
  EXPECT_EQ(uart.tx[5], SCS_REG_PRESENT_POSITION);
  EXPECT_EQ(uart.tx[6], SCS_FEEDBACK_LENGTH);
}

TEST(SCS9009BusTest, DispatchesTheResponseToTheServo) {
  TestUART uart;
  SCS9009Component bus;
  bus.set_uart_parent(&uart);
  SCS9009Servo servo;
  servo.set_address(1);
  servo.set_parent(&bus);

  {
    LoopBlockingGuard clock{&bus, LOG_STR("test"), 1000};
    ASSERT_TRUE(bus.queue_read(&servo, 1, SCS_REG_PRESENT_POSITION, SCS_FEEDBACK_LENGTH, true));
    bus.loop();
  }
  feed(uart, feedback_response(1, 0));
  {
    LoopBlockingGuard clock{&bus, LOG_STR("test"), 1020};
    bus.loop();
  }

  ASSERT_TRUE(servo.has_feedback());
  EXPECT_EQ(servo.feedback().position, 500);
  EXPECT_EQ(servo.feedback().speed, 100);
  EXPECT_EQ(servo.feedback().load, -10);
}

TEST(SCS9009BusTest, IgnoresAnEchoOfItsOwnFrame) {
  TestUART uart;
  SCS9009Component bus;
  bus.set_uart_parent(&uart);
  SCS9009Servo servo;
  servo.set_address(1);
  servo.set_parent(&bus);

  {
    LoopBlockingGuard clock{&bus, LOG_STR("test"), 1000};
    ASSERT_TRUE(bus.queue_read(&servo, 1, SCS_REG_PRESENT_POSITION, SCS_FEEDBACK_LENGTH, true));
    bus.loop();
  }
  feed(uart, uart.tx);  // the transceiver loops the request back
  {
    LoopBlockingGuard clock{&bus, LOG_STR("test"), 1010};
    bus.loop();
  }
  EXPECT_FALSE(servo.has_feedback());

  feed(uart, feedback_response(1, 0));
  {
    LoopBlockingGuard clock{&bus, LOG_STR("test"), 1020};
    bus.loop();
  }
  EXPECT_TRUE(servo.has_feedback());
}

TEST(SCS9009BusTest, RecoversAfterATimeout) {
  TestUART uart;
  SCS9009Component bus;
  bus.set_uart_parent(&uart);
  SCS9009Servo servo;
  servo.set_address(1);
  servo.set_parent(&bus);

  {
    LoopBlockingGuard clock{&bus, LOG_STR("test"), 1000};
    ASSERT_TRUE(bus.queue_read(&servo, 1, SCS_REG_PRESENT_POSITION, SCS_FEEDBACK_LENGTH, true));
    bus.loop();
  }
  ASSERT_EQ(uart.tx.size(), 8);

  {
    LoopBlockingGuard clock{&bus, LOG_STR("test"), 1011};  // timeout is 10 ms
    bus.loop();
  }
  EXPECT_FALSE(servo.has_feedback());

  {
    LoopBlockingGuard clock{&bus, LOG_STR("test"), 1020};
    ASSERT_TRUE(bus.queue_read(&servo, 1, SCS_REG_PRESENT_POSITION, SCS_FEEDBACK_LENGTH, true));
    bus.loop();
  }
  EXPECT_EQ(uart.tx.size(), 16);
}

TEST(SCS9009BusTest, WriteWithoutResponseCompletesAfterTransmission) {
  TestUART uart;
  SCS9009Component bus;
  bus.set_uart_parent(&uart);
  SCS9009Servo servo;
  servo.set_address(1);
  servo.set_parent(&bus);

  const uint8_t value = 1;
  {
    LoopBlockingGuard clock{&bus, LOG_STR("test"), 1000};
    ASSERT_TRUE(bus.queue_write(&servo, 1, SCS_REG_TORQUE_ENABLE, &value, 1, false));
    bus.loop();
  }
  ASSERT_EQ(uart.tx.size(), 8);
  EXPECT_EQ(uart.tx[3], 4);
  EXPECT_EQ(uart.tx[4], SCS_INSTRUCTION_WRITE);
  EXPECT_EQ(uart.tx[5], SCS_REG_TORQUE_ENABLE);
  EXPECT_EQ(uart.tx[6], 1);

  {
    LoopBlockingGuard clock{&bus, LOG_STR("test"), 1010};
    ASSERT_TRUE(bus.queue_write(&servo, 1, SCS_REG_TORQUE_ENABLE, &value, 1, false));
    bus.loop();
  }
  EXPECT_EQ(uart.tx.size(), 16);
}

TEST(SCS9009BusTest, AcknowledgedWriteCompletes) {
  TestUART uart;
  SCS9009Component bus;
  bus.set_uart_parent(&uart);
  SCS9009Servo servo;
  servo.set_address(1);
  servo.set_parent(&bus);

  const uint8_t value = 1;
  {
    LoopBlockingGuard clock{&bus, LOG_STR("test"), 1000};
    ASSERT_TRUE(bus.queue_write(&servo, 1, SCS_REG_TORQUE_ENABLE, &value, 1, true));
    bus.loop();
  }
  ASSERT_EQ(uart.tx.size(), 8);

  // ACK for address 1: FF FF 01 02 00 FC
  feed(uart, {0xFF, 0xFF, 0x01, 0x02, 0x00, 0xFC});
  {
    LoopBlockingGuard clock{&bus, LOG_STR("test"), 1020};
    bus.loop();
  }
  EXPECT_FALSE(servo.status_has_warning());

  {
    LoopBlockingGuard clock{&bus, LOG_STR("test"), 1030};
    ASSERT_TRUE(bus.queue_write(&servo, 1, SCS_REG_TORQUE_ENABLE, &value, 1, true));
    bus.loop();
  }
  EXPECT_EQ(uart.tx.size(), 16);
}

TEST(SCS9009BusTest, SendsOneSyncWriteFrameForAllServos) {
  TestUART uart;
  SCS9009Component bus;
  bus.set_uart_parent(&uart);
  SCS9009Servo first;
  SCS9009Servo second;
  first.set_address(1);
  first.set_parent(&bus);
  second.set_address(2);
  second.set_parent(&bus);

  const uint8_t first_data[6] = {0x00, 0x08, 0x00, 0x00, 0xE8, 0x03};
  const uint8_t second_data[6] = {0x01, 0x2C, 0x00, 0x00, 0xE8, 0x03};
  {
    LoopBlockingGuard clock{&bus, LOG_STR("test"), 1000};
    ASSERT_TRUE(bus.begin_sync_write());
    ASSERT_TRUE(bus.add_sync_write(&first, 1, SCS_REG_GOAL_POSITION, first_data, sizeof(first_data)));
    ASSERT_TRUE(bus.add_sync_write(&second, 2, SCS_REG_GOAL_POSITION, second_data, sizeof(second_data)));
    ASSERT_TRUE(bus.end_sync_write());
    bus.loop();
  }

  ASSERT_EQ(uart.tx.size(), 8 + 2 * 7);
  EXPECT_EQ(uart.tx[2], SCS_BROADCAST_ADDRESS);
  EXPECT_EQ(uart.tx[3], 18);  // (data length + id) * servos + 4
  EXPECT_EQ(uart.tx[4], SCS_INSTRUCTION_SYNC_WRITE);
  EXPECT_EQ(uart.tx[5], SCS_REG_GOAL_POSITION);
  EXPECT_EQ(uart.tx[6], 6);
  EXPECT_EQ(uart.tx[7], 1);
  EXPECT_EQ(uart.tx[14], 2);
  EXPECT_EQ(uart.tx[15], 0x01);
  EXPECT_EQ(uart.tx[16], 0x2C);
  EXPECT_FALSE(first.status_has_warning());
  EXPECT_FALSE(second.status_has_warning());
}

TEST(SCS9009BusTest, RejectsMixedSyncWriteLayouts) {
  TestUART uart;
  SCS9009Component bus;
  bus.set_uart_parent(&uart);
  SCS9009Servo first;
  SCS9009Servo second;
  first.set_parent(&bus);
  second.set_parent(&bus);
  const uint8_t data[6] = {};

  ASSERT_TRUE(bus.begin_sync_write());
  ASSERT_TRUE(bus.add_sync_write(&first, 1, SCS_REG_GOAL_POSITION, data, sizeof(data)));
  EXPECT_FALSE(bus.add_sync_write(&second, 2, SCS_REG_TORQUE_ENABLE, data, 1));
  EXPECT_FALSE(bus.add_sync_write(&second, 2, SCS_REG_GOAL_POSITION, data, 2));
  bus.abort_sync_write();
  EXPECT_FALSE(bus.sync_write_active());

  {
    LoopBlockingGuard clock{&bus, LOG_STR("test"), 1000};
    bus.loop();
  }
  EXPECT_TRUE(uart.tx.empty());
}

TEST(SCS9009BusTest, SyncReadDispatchesAllResponsesInOneLoop) {
  TestUART uart;
  SCS9009Component bus;
  bus.set_uart_parent(&uart);
  SCS9009Servo first;
  SCS9009Servo second;
  first.set_address(1);
  first.set_parent(&bus);
  second.set_address(2);
  second.set_parent(&bus);
  SCS9009Servo *servos[2] = {&first, &second};

  {
    LoopBlockingGuard clock{&bus, LOG_STR("test"), 1000};
    ASSERT_TRUE(bus.queue_sync_read(servos, 2, SCS_REG_PRESENT_POSITION, SCS_FEEDBACK_LENGTH));
    bus.loop();
  }

  ASSERT_EQ(uart.tx.size(), 10);
  EXPECT_EQ(uart.tx[2], SCS_BROADCAST_ADDRESS);
  EXPECT_EQ(uart.tx[3], 6);
  EXPECT_EQ(uart.tx[4], SCS_INSTRUCTION_SYNC_READ);
  EXPECT_EQ(uart.tx[5], SCS_REG_PRESENT_POSITION);
  EXPECT_EQ(uart.tx[6], SCS_FEEDBACK_LENGTH);
  EXPECT_EQ(uart.tx[7], 1);
  EXPECT_EQ(uart.tx[8], 2);

  feed(uart, feedback_response(1, 0, 400));
  feed(uart, feedback_response(2, 0, 700));
  {
    LoopBlockingGuard clock{&bus, LOG_STR("test"), 1020};
    bus.loop();
  }

  ASSERT_TRUE(first.has_feedback());
  ASSERT_TRUE(second.has_feedback());
  EXPECT_EQ(first.feedback().position, 400);
  EXPECT_EQ(second.feedback().position, 700);
}

TEST(SCS9009BusTest, SyncReadFailsTheServosThatDidNotAnswer) {
  TestUART uart;
  SCS9009Component bus;
  bus.set_uart_parent(&uart);
  SCS9009Servo first;
  SCS9009Servo second;
  first.set_address(1);
  first.set_parent(&bus);
  second.set_address(2);
  second.set_parent(&bus);
  SCS9009Servo *servos[2] = {&first, &second};

  {
    LoopBlockingGuard clock{&bus, LOG_STR("test"), 1000};
    ASSERT_TRUE(bus.queue_sync_read(servos, 2, SCS_REG_PRESENT_POSITION, SCS_FEEDBACK_LENGTH));
    bus.loop();
  }
  feed(uart, feedback_response(1, 0, 400));

  {
    LoopBlockingGuard clock{&bus, LOG_STR("test"), 1011};
    bus.loop();
  }
  EXPECT_TRUE(first.has_feedback());
  EXPECT_FALSE(second.has_feedback());

  {
    LoopBlockingGuard clock{&bus, LOG_STR("test"), 1022};
    bus.loop();
  }
  EXPECT_FALSE(second.has_feedback());
  EXPECT_TRUE(second.status_has_warning());
  EXPECT_FALSE(first.status_has_warning());
}

TEST(SCS9009BusTest, GroupMoveSendsOneSyncWrite) {
  TestUART uart;
  SCS9009Component bus;
  bus.set_uart_parent(&uart);
  SCS9009Servo first;
  SCS9009Servo second;
  first.set_address(1);
  first.set_parent(&bus);
  second.set_address(2);
  second.set_parent(&bus);

  ftservo::FTServoGroup group;
  group.set_expected_servos(2);
  ASSERT_TRUE(group.add_servo(&first));
  ASSERT_TRUE(group.add_servo(&second));

  // A move enables the torque of its axes first, so take those frames off the bus
  // before the grouped move itself goes out.
  {
    LoopBlockingGuard clock{&bus, LOG_STR("test"), 1000};
    ASSERT_TRUE(first.set_torque(true));
    ASSERT_TRUE(second.set_torque(true));
    bus.loop();
  }
  uart.tx.clear();
  {
    LoopBlockingGuard clock{&bus, LOG_STR("test"), 1010};
    bus.loop();
  }
  uart.tx.clear();

  const ftservo::ServoGoal goals[2] = {{600, 500, 0}, {700, 500, 0}};
  {
    LoopBlockingGuard clock{&bus, LOG_STR("test"), 1020};
    ASSERT_TRUE(group.move_to(goals, 2));
    bus.loop();
  }

  ASSERT_EQ(uart.tx.size(), 22);
  EXPECT_EQ(uart.tx[2], SCS_BROADCAST_ADDRESS);
  EXPECT_EQ(uart.tx[3], 18);
  EXPECT_EQ(uart.tx[4], SCS_INSTRUCTION_SYNC_WRITE);
  EXPECT_EQ(uart.tx[5], SCS_REG_GOAL_POSITION);
  EXPECT_EQ(uart.tx[6], 6);
  EXPECT_EQ(uart.tx[7], 1);
  EXPECT_EQ(uart.tx[8], 0x02);  // 600, big endian
  EXPECT_EQ(uart.tx[9], 0x58);
  EXPECT_EQ(uart.tx[12], 0x01);  // speed 500
  EXPECT_EQ(uart.tx[13], 0xF4);
  EXPECT_EQ(uart.tx[14], 2);
  EXPECT_EQ(uart.tx[15], 0x02);  // 700
  EXPECT_EQ(uart.tx[16], 0xBC);
  EXPECT_EQ(first.state(), ftservo::ServoState::SERVO_STATE_MOVING);
  EXPECT_EQ(second.state(), ftservo::ServoState::SERVO_STATE_MOVING);
  EXPECT_FALSE(first.status_has_warning());
  EXPECT_FALSE(second.status_has_warning());
}

}  // namespace esphome::scs9009::testing
