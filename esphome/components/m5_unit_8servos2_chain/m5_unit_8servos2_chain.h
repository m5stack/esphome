#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>

#include "esphome/components/m5_chain_base/m5_chain_base.h"

namespace esphome::switch_ {
class Switch;
}

namespace esphome::m5_unit_8servos2_chain {

/// The unit drives eight Grove channels and up to sixteen LEDs in total.
static constexpr uint8_t SERVOS2_CHANNEL_COUNT = 8;
static constexpr uint8_t SERVOS2_RGB_COUNT = 16;
/// Highest servo angle in degrees and highest PWM duty in percent.
static constexpr uint8_t SERVOS2_SERVO_ANGLE_MAX = 180;
static constexpr uint8_t SERVOS2_PWM_DUTY_MAX = 100;
/// Most LEDs a single channel can drive.
static constexpr uint8_t SERVOS2_RGB_PER_CHANNEL_MAX = 16;
/// Most LEDs the whole unit can drive.
static constexpr uint8_t SERVOS2_RGB_TOTAL_MAX = 16;
/// Set in the RGB configuration byte to push the buffered colours out.
static constexpr uint8_t SERVOS2_RGB_CONFIG_REFRESH = 0x20;
/// Requested-mode slot that no sub-platform has claimed yet.
static constexpr uint8_t SERVOS2_MODE_NONE = 0xFF;
/// Back off after a rejected write so a repeating failure does not flood the bus.
static constexpr uint32_t SERVOS2_WRITE_BACKOFF_MS = 250;

enum class Servos2Command : uint8_t {
  SERVOS2_COMMAND_SET_MODE = 0x10,
  SERVOS2_COMMAND_SET_MODE_ALL = 0x11,
  SERVOS2_COMMAND_GET_MODE = 0x12,
  SERVOS2_COMMAND_GET_MODE_ALL = 0x13,
  SERVOS2_COMMAND_SET_INPUT_PULL = 0x30,
  SERVOS2_COMMAND_SET_OUTPUT_LEVEL = 0x40,
  SERVOS2_COMMAND_SET_OUTPUT_LEVEL_ALL = 0x41,
  SERVOS2_COMMAND_SET_SERVO_ANGLE = 0x60,
  SERVOS2_COMMAND_SET_SERVO_ANGLE_ALL = 0x61,
  SERVOS2_COMMAND_SET_RGB_CONFIG = 0x70,
  SERVOS2_COMMAND_SET_RGB_BUFFER = 0x74,
  SERVOS2_COMMAND_SET_PWM_DUTY = 0x80,
  SERVOS2_COMMAND_SET_PWM_DUTY_ALL = 0x81,
};

/// One function per channel; the channel has to be switched to the matching mode first.
enum class Servos2Mode : uint8_t {
  SERVOS2_MODE_INPUT = 0x00,
  SERVOS2_MODE_OUTPUT = 0x01,
  SERVOS2_MODE_ADC = 0x02,
  SERVOS2_MODE_SERVO = 0x03,
  SERVOS2_MODE_RGB = 0x04,
  SERVOS2_MODE_PWM = 0x05,
};

enum class Servos2Pull : uint8_t {
  SERVOS2_PULL_NONE = 0x00,
  SERVOS2_PULL_UP = 0x01,
  SERVOS2_PULL_DOWN = 0x02,
};

/// Output kinds the sub-platforms drive through the shared shadow state.
enum class Servos2OutputType : uint8_t {
  SERVOS2_OUTPUT_TYPE_SERVO = 0,
  SERVOS2_OUTPUT_TYPE_PWM = 1,
};

class M5Unit8Servos2Chain : public m5_chain_base::M5ChainBase {
 public:
  M5Unit8Servos2Chain();

  void setup() override;
  void loop() override;
  void dump_config() override;

  m5_chain_base::ChainStatus set_mode(uint8_t channel, Servos2Mode mode);
  m5_chain_base::ChainStatus set_servo_angle(uint8_t channel, uint8_t angle);
  m5_chain_base::ChainStatus set_servo_angles(std::initializer_list<uint8_t> angles);
  m5_chain_base::ChainStatus set_servo_angles(const std::array<uint8_t, SERVOS2_CHANNEL_COUNT> &angles);
  m5_chain_base::ChainStatus set_pwm_duty(uint8_t channel, uint8_t duty);
  m5_chain_base::ChainStatus set_pwm_duties(std::initializer_list<uint8_t> duties);
  m5_chain_base::ChainStatus set_pwm_duties(const std::array<uint8_t, SERVOS2_CHANNEL_COUNT> &duties);
  m5_chain_base::ChainStatus set_output_level(uint8_t channel, bool level);
  m5_chain_base::ChainStatus set_output_levels(std::initializer_list<bool> levels);
  m5_chain_base::ChainStatus set_output_levels(const std::array<bool, SERVOS2_CHANNEL_COUNT> &levels);
  m5_chain_base::ChainStatus set_input_pull(uint8_t channel, Servos2Pull pull);
  /// Number of LEDs on a channel, in the range 1 to 16, and whether to push the buffered colours out.
  m5_chain_base::ChainStatus set_rgb_config(uint8_t channel, uint8_t led_count, bool refresh);
  /// Colour of one LED in the buffer, addressed across all channels.
  m5_chain_base::ChainStatus set_rgb(uint8_t index, uint8_t red, uint8_t green, uint8_t blue);

  /// Desired-state API used by the sub-platform entities. The hub sends it once the bus is free and
  /// always applies a channel's mode before its value.
  void request_mode(uint8_t channel, Servos2Mode mode);
  void set_servo_angle_desired(uint8_t channel, uint8_t angle);
  void set_pwm_duty_desired(uint8_t channel, uint8_t duty);
  void set_output_level_desired(uint8_t channel, bool level);
  void set_level_switch(uint8_t channel, switch_::Switch *level_switch);
  /// Queue one light buffer; the hub writes it out and latches it with a refresh config frame.
  void set_rgb_buffer(uint8_t channel, uint8_t offset, uint8_t count, const uint8_t *buffer);

 protected:
  void on_response(const m5_chain_base::ChainFrame &frame) override;
  void on_unsolicited(const m5_chain_base::ChainFrame &frame) override;
  void on_request_timeout(uint8_t index, uint8_t command) override;

  void poll_next_() override;
  void handle_modes_(const m5_chain_base::ChainFrame &frame);

  m5_chain_base::ChainStatus set_servo_angles_(const uint8_t *angles, size_t count);
  m5_chain_base::ChainStatus set_pwm_duties_(const uint8_t *duties, size_t count);
  m5_chain_base::ChainStatus set_output_levels_(const bool *levels, size_t count);
  /// Send at most one queued operation; returns true when a request was started.
  bool flush_next_();
  void publish_levels_();

  enum class FlushOp : uint8_t {
    FLUSH_OP_NONE = 0,
    FLUSH_OP_MODE,
    FLUSH_OP_SERVO,
    FLUSH_OP_PWM,
    FLUSH_OP_LEVEL,
    FLUSH_OP_RGB_BUFFER,
    FLUSH_OP_RGB_LATCH,
  };

  struct RgbFlushState {
    const uint8_t *buffer{nullptr};
    uint8_t offset{0};
    uint8_t count{0};
    uint8_t next{0};
    bool pending{false};
    uint16_t generation{0};
    uint16_t flushing_generation{0};
  };

  std::array<uint8_t, SERVOS2_CHANNEL_COUNT> desired_servo_angles_{};
  std::array<uint8_t, SERVOS2_CHANNEL_COUNT> desired_pwm_duties_{};
  std::array<bool, SERVOS2_CHANNEL_COUNT> desired_levels_{};
  std::array<uint8_t, SERVOS2_CHANNEL_COUNT> requested_modes_{};
  std::array<uint8_t, SERVOS2_CHANNEL_COUNT> applied_modes_{};
  std::array<switch_::Switch *, SERVOS2_CHANNEL_COUNT> level_switches_{};
  std::array<RgbFlushState, SERVOS2_CHANNEL_COUNT> rgb_flush_{};

  std::array<uint8_t, SERVOS2_CHANNEL_COUNT> pending_servo_angles_{};
  std::array<uint8_t, SERVOS2_CHANNEL_COUNT> pending_pwm_duties_{};
  std::array<bool, SERVOS2_CHANNEL_COUNT> pending_levels_{};

  uint16_t servo_generation_{0};
  uint16_t pwm_generation_{0};
  uint16_t level_generation_{0};
  uint16_t sent_servo_generation_{0};
  uint16_t sent_pwm_generation_{0};
  uint16_t sent_level_generation_{0};

  bool servo_dirty_{false};
  bool pwm_dirty_{false};
  bool levels_dirty_{false};
  uint8_t pending_mode_{SERVOS2_MODE_NONE};
  FlushOp pending_op_{FlushOp::FLUSH_OP_NONE};
  uint8_t pending_channel_{0};
  uint32_t next_flush_at_{0};
  bool timeout_warning_active_{false};
};

}  // namespace esphome::m5_unit_8servos2_chain
