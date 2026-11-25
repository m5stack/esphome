#pragma once

#include "esphome/components/ftservo/ftservo.h"
#include "esphome/core/component.h"

#include "scs9009.h"

namespace esphome::scs9009 {

// SCS/SCSCL memory table
static constexpr uint8_t SCS_REG_ENDING = 2;
static constexpr uint8_t SCS_REG_RESPONSE_LEVEL = 8;
static constexpr uint8_t SCS_REG_TORQUE_ENABLE = 40;
static constexpr uint8_t SCS_REG_GOAL_POSITION = 42;
static constexpr uint8_t SCS_REG_PRESENT_POSITION = 56;
static constexpr uint8_t SCS_REG_PRESENT_SPEED = 58;
static constexpr uint8_t SCS_REG_PRESENT_LOAD = 60;
static constexpr uint8_t SCS_REG_PRESENT_VOLTAGE = 62;
static constexpr uint8_t SCS_REG_PRESENT_TEMPERATURE = 63;
static constexpr uint8_t SCS_REG_PRESENT_ASYNC = 64;
static constexpr uint8_t SCS_REG_STATUS = 65;
static constexpr uint8_t SCS_REG_PRESENT_MOVING = 66;

static_assert(SCS_REG_PRESENT_ASYNC > SCS_REG_PRESENT_TEMPERATURE && SCS_REG_PRESENT_ASYNC < SCS_REG_PRESENT_MOVING,
              "The feedback read covers registers 56 to 66 in order");

static constexpr uint8_t SCS_INIT_LENGTH = SCS_REG_RESPONSE_LEVEL - SCS_REG_ENDING + 1;
static constexpr uint8_t SCS_FEEDBACK_LENGTH = SCS_REG_PRESENT_MOVING - SCS_REG_PRESENT_POSITION + 1;

// Status bits reported in the response and in the status register
static constexpr uint8_t SCS_STATUS_VOLTAGE = 1 << 0;
static constexpr uint8_t SCS_STATUS_TEMPERATURE = 1 << 2;
static constexpr uint8_t SCS_STATUS_OVERLOAD = 1 << 5;

class SCS9009Servo : public ftservo::FTServo, public Parented<SCS9009Component> {
 public:
  void setup() override;
  void dump_config() override;

  void set_address(uint8_t address) { this->address_ = address; }
  uint8_t address() const { return this->address_; }

  // Called by the bus when a transaction completes.
  void on_scs_read(uint8_t reg, const uint8_t *data, uint8_t length, uint8_t status);
  void on_scs_read_failed(ftservo::ServoErrorMask error);
  void on_scs_write(bool ok, ftservo::ServoErrorMask error);

 protected:
  bool write_goal_(uint16_t position, uint16_t speed, uint16_t time) override;
  bool write_torque_(bool enabled) override;
  bool request_feedback_() override;

  // Group support through the bus SYNC WRITE.
  bool supports_group_write_() const override { return true; }
  bool begin_group_write_() override;
  bool end_group_write_() override;
  void abort_group_write_() override;
  const void *transport_id_() const override;

  uint8_t address_{0};
  bool big_endian_{true};
  bool init_pending_{false};
  uint8_t response_level_{0};
};

}  // namespace esphome::scs9009
