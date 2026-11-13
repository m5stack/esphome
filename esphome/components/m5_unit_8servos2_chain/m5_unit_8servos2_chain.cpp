#include "m5_unit_8servos2_chain.h"

#include <algorithm>
#include <array>

#include "esphome/components/switch/switch.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::m5_unit_8servos2_chain {

ESPHOME_LOG_TAG(TAG, "m5_unit_8servos2_chain");

static constexpr uint8_t SERVOS2_COMMAND_SET_MODE = static_cast<uint8_t>(Servos2Command::SERVOS2_COMMAND_SET_MODE);
static constexpr uint8_t SERVOS2_COMMAND_GET_MODE_ALL =
    static_cast<uint8_t>(Servos2Command::SERVOS2_COMMAND_GET_MODE_ALL);
static constexpr uint8_t SERVOS2_COMMAND_SET_INPUT_PULL =
    static_cast<uint8_t>(Servos2Command::SERVOS2_COMMAND_SET_INPUT_PULL);
static constexpr uint8_t SERVOS2_COMMAND_SET_OUTPUT_LEVEL =
    static_cast<uint8_t>(Servos2Command::SERVOS2_COMMAND_SET_OUTPUT_LEVEL);
static constexpr uint8_t SERVOS2_COMMAND_SET_OUTPUT_LEVEL_ALL =
    static_cast<uint8_t>(Servos2Command::SERVOS2_COMMAND_SET_OUTPUT_LEVEL_ALL);
static constexpr uint8_t SERVOS2_COMMAND_SET_SERVO_ANGLE =
    static_cast<uint8_t>(Servos2Command::SERVOS2_COMMAND_SET_SERVO_ANGLE);
static constexpr uint8_t SERVOS2_COMMAND_SET_SERVO_ANGLE_ALL =
    static_cast<uint8_t>(Servos2Command::SERVOS2_COMMAND_SET_SERVO_ANGLE_ALL);
static constexpr uint8_t SERVOS2_COMMAND_SET_RGB_CONFIG =
    static_cast<uint8_t>(Servos2Command::SERVOS2_COMMAND_SET_RGB_CONFIG);
static constexpr uint8_t SERVOS2_COMMAND_SET_RGB_BUFFER =
    static_cast<uint8_t>(Servos2Command::SERVOS2_COMMAND_SET_RGB_BUFFER);
static constexpr uint8_t SERVOS2_COMMAND_SET_PWM_DUTY =
    static_cast<uint8_t>(Servos2Command::SERVOS2_COMMAND_SET_PWM_DUTY);
static constexpr uint8_t SERVOS2_COMMAND_SET_PWM_DUTY_ALL =
    static_cast<uint8_t>(Servos2Command::SERVOS2_COMMAND_SET_PWM_DUTY_ALL);

void M5Unit8Servos2Chain::setup() {
  ESP_LOGCONFIG(TAG, "Setting up M5Stack Unit 8Servos2 Chain (chain_index=%u)...", this->get_chain_index());
}

M5Unit8Servos2Chain::M5Unit8Servos2Chain() {
  this->requested_modes_.fill(SERVOS2_MODE_NONE);
  this->applied_modes_.fill(SERVOS2_MODE_NONE);
}

void M5Unit8Servos2Chain::loop() {
  m5_chain_base::M5ChainBase::loop();
  if (this->request_pending())
    return;
  const uint32_t now = App.get_loop_component_start_time();
  if (static_cast<int32_t>(now - this->next_flush_at_) < 0)
    return;
  this->flush_next_();
}

void M5Unit8Servos2Chain::dump_config() {
  this->dump_base_config();
  ESP_LOGCONFIG(TAG, "M5Stack Unit 8Servos2 Chain:");
  ESP_LOGCONFIG(TAG, "  Chain index: %u", this->get_chain_index());
}

void M5Unit8Servos2Chain::poll_next_() {
  if (this->request_pending())
    return;
  if (this->request(this->get_chain_index(), SERVOS2_COMMAND_GET_MODE_ALL) !=
      m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
    this->status_set_warning();
}

m5_chain_base::ChainStatus M5Unit8Servos2Chain::set_mode(uint8_t channel, Servos2Mode mode) {
  if (channel >= SERVOS2_CHANNEL_COUNT)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {channel, static_cast<uint8_t>(mode)};
  return this->request(this->get_chain_index(), SERVOS2_COMMAND_SET_MODE, data, sizeof(data));
}

m5_chain_base::ChainStatus M5Unit8Servos2Chain::set_servo_angle(uint8_t channel, uint8_t angle) {
  if (channel >= SERVOS2_CHANNEL_COUNT || angle > SERVOS2_SERVO_ANGLE_MAX)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {channel, angle};
  return this->request(this->get_chain_index(), SERVOS2_COMMAND_SET_SERVO_ANGLE, data, sizeof(data));
}

m5_chain_base::ChainStatus M5Unit8Servos2Chain::set_servo_angles(std::initializer_list<uint8_t> angles) {
  return this->set_servo_angles_(angles.begin(), angles.size());
}

m5_chain_base::ChainStatus M5Unit8Servos2Chain::set_servo_angles(
    const std::array<uint8_t, SERVOS2_CHANNEL_COUNT> &angles) {
  return this->set_servo_angles_(angles.data(), angles.size());
}

m5_chain_base::ChainStatus M5Unit8Servos2Chain::set_servo_angles_(const uint8_t *angles, size_t count) {
  if (count != SERVOS2_CHANNEL_COUNT)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  for (size_t i = 0; i < count; i++) {
    if (angles[i] > SERVOS2_SERVO_ANGLE_MAX)
      return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  }
  return this->request(this->get_chain_index(), SERVOS2_COMMAND_SET_SERVO_ANGLE_ALL, angles,
                       static_cast<uint16_t>(count));
}

m5_chain_base::ChainStatus M5Unit8Servos2Chain::set_pwm_duty(uint8_t channel, uint8_t duty) {
  if (channel >= SERVOS2_CHANNEL_COUNT || duty > SERVOS2_PWM_DUTY_MAX)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {channel, duty};
  return this->request(this->get_chain_index(), SERVOS2_COMMAND_SET_PWM_DUTY, data, sizeof(data));
}

m5_chain_base::ChainStatus M5Unit8Servos2Chain::set_pwm_duties(std::initializer_list<uint8_t> duties) {
  return this->set_pwm_duties_(duties.begin(), duties.size());
}

m5_chain_base::ChainStatus M5Unit8Servos2Chain::set_pwm_duties(
    const std::array<uint8_t, SERVOS2_CHANNEL_COUNT> &duties) {
  return this->set_pwm_duties_(duties.data(), duties.size());
}

m5_chain_base::ChainStatus M5Unit8Servos2Chain::set_pwm_duties_(const uint8_t *duties, size_t count) {
  if (count != SERVOS2_CHANNEL_COUNT)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  for (size_t i = 0; i < count; i++) {
    if (duties[i] > SERVOS2_PWM_DUTY_MAX)
      return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  }
  return this->request(this->get_chain_index(), SERVOS2_COMMAND_SET_PWM_DUTY_ALL, duties, static_cast<uint16_t>(count));
}

m5_chain_base::ChainStatus M5Unit8Servos2Chain::set_output_level(uint8_t channel, bool level) {
  if (channel >= SERVOS2_CHANNEL_COUNT)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {channel, static_cast<uint8_t>(level)};
  return this->request(this->get_chain_index(), SERVOS2_COMMAND_SET_OUTPUT_LEVEL, data, sizeof(data));
}

m5_chain_base::ChainStatus M5Unit8Servos2Chain::set_output_levels(std::initializer_list<bool> levels) {
  return this->set_output_levels_(levels.begin(), levels.size());
}

m5_chain_base::ChainStatus M5Unit8Servos2Chain::set_output_levels(
    const std::array<bool, SERVOS2_CHANNEL_COUNT> &levels) {
  return this->set_output_levels_(levels.data(), levels.size());
}

m5_chain_base::ChainStatus M5Unit8Servos2Chain::set_output_levels_(const bool *levels, size_t count) {
  if (count != SERVOS2_CHANNEL_COUNT)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  std::array<uint8_t, SERVOS2_CHANNEL_COUNT> data{};
  std::transform(levels, levels + count, data.begin(), [](bool level) { return static_cast<uint8_t>(level); });
  return this->request(this->get_chain_index(), SERVOS2_COMMAND_SET_OUTPUT_LEVEL_ALL, data.data(),
                       static_cast<uint16_t>(data.size()));
}

m5_chain_base::ChainStatus M5Unit8Servos2Chain::set_input_pull(uint8_t channel, Servos2Pull pull) {
  if (channel >= SERVOS2_CHANNEL_COUNT)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {channel, static_cast<uint8_t>(pull)};
  return this->request(this->get_chain_index(), SERVOS2_COMMAND_SET_INPUT_PULL, data, sizeof(data));
}

m5_chain_base::ChainStatus M5Unit8Servos2Chain::set_rgb_config(uint8_t channel, uint8_t led_count, bool refresh) {
  if (channel >= SERVOS2_CHANNEL_COUNT || led_count == 0 || led_count > SERVOS2_RGB_PER_CHANNEL_MAX)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  uint8_t config = led_count;
  if (refresh)
    config |= SERVOS2_RGB_CONFIG_REFRESH;
  const uint8_t data[] = {channel, config};
  return this->request(this->get_chain_index(), SERVOS2_COMMAND_SET_RGB_CONFIG, data, sizeof(data));
}

m5_chain_base::ChainStatus M5Unit8Servos2Chain::set_rgb(uint8_t index, uint8_t red, uint8_t green, uint8_t blue) {
  if (index >= SERVOS2_RGB_COUNT)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {index, red, green, blue};
  return this->request(this->get_chain_index(), SERVOS2_COMMAND_SET_RGB_BUFFER, data, sizeof(data));
}

void M5Unit8Servos2Chain::request_mode(uint8_t channel, Servos2Mode mode) {
  if (channel < SERVOS2_CHANNEL_COUNT)
    this->requested_modes_[channel] = static_cast<uint8_t>(mode);
}

void M5Unit8Servos2Chain::set_servo_angle_desired(uint8_t channel, uint8_t angle) {
  if (channel >= SERVOS2_CHANNEL_COUNT || angle > SERVOS2_SERVO_ANGLE_MAX)
    return;
  this->desired_servo_angles_[channel] = angle;
  this->servo_dirty_ = true;
  this->servo_generation_++;
}

void M5Unit8Servos2Chain::set_pwm_duty_desired(uint8_t channel, uint8_t duty) {
  if (channel >= SERVOS2_CHANNEL_COUNT || duty > SERVOS2_PWM_DUTY_MAX)
    return;
  this->desired_pwm_duties_[channel] = duty;
  this->pwm_dirty_ = true;
  this->pwm_generation_++;
}

void M5Unit8Servos2Chain::set_output_level_desired(uint8_t channel, bool level) {
  if (channel >= SERVOS2_CHANNEL_COUNT)
    return;
  this->desired_levels_[channel] = level;
  this->levels_dirty_ = true;
  this->level_generation_++;
}

void M5Unit8Servos2Chain::set_level_switch(uint8_t channel, switch_::Switch *level_switch) {
  if (channel < SERVOS2_CHANNEL_COUNT)
    this->level_switches_[channel] = level_switch;
}

void M5Unit8Servos2Chain::set_rgb_buffer(uint8_t channel, uint8_t offset, uint8_t count, const uint8_t *buffer) {
  if (channel >= SERVOS2_CHANNEL_COUNT || buffer == nullptr || count == 0 || count > SERVOS2_RGB_PER_CHANNEL_MAX ||
      offset + count > SERVOS2_RGB_TOTAL_MAX)
    return;
  auto &state = this->rgb_flush_[channel];
  if (!state.pending)
    state.next = 0;
  state.buffer = buffer;
  state.offset = offset;
  state.count = count;
  state.generation++;
  state.pending = true;
}

bool M5Unit8Servos2Chain::flush_next_() {
  for (uint8_t channel = 0; channel < SERVOS2_CHANNEL_COUNT; channel++) {
    const uint8_t requested = this->requested_modes_[channel];
    if (requested == SERVOS2_MODE_NONE || requested == this->applied_modes_[channel])
      continue;
    this->pending_op_ = FlushOp::FLUSH_OP_MODE;
    this->pending_channel_ = channel;
    this->pending_mode_ = requested;
    if (this->set_mode(channel, static_cast<Servos2Mode>(requested)) != m5_chain_base::ChainStatus::CHAIN_STATUS_OK) {
      this->pending_op_ = FlushOp::FLUSH_OP_NONE;
      return false;
    }
    return true;
  }

  if (this->servo_dirty_) {
    this->pending_servo_angles_ = this->desired_servo_angles_;
    this->sent_servo_generation_ = this->servo_generation_;
    this->pending_op_ = FlushOp::FLUSH_OP_SERVO;
    if (this->set_servo_angles(this->pending_servo_angles_) != m5_chain_base::ChainStatus::CHAIN_STATUS_OK) {
      this->pending_op_ = FlushOp::FLUSH_OP_NONE;
      return false;
    }
    return true;
  }

  if (this->pwm_dirty_) {
    this->pending_pwm_duties_ = this->desired_pwm_duties_;
    this->sent_pwm_generation_ = this->pwm_generation_;
    this->pending_op_ = FlushOp::FLUSH_OP_PWM;
    if (this->set_pwm_duties(this->pending_pwm_duties_) != m5_chain_base::ChainStatus::CHAIN_STATUS_OK) {
      this->pending_op_ = FlushOp::FLUSH_OP_NONE;
      return false;
    }
    return true;
  }

  if (this->levels_dirty_) {
    this->pending_levels_ = this->desired_levels_;
    this->sent_level_generation_ = this->level_generation_;
    this->pending_op_ = FlushOp::FLUSH_OP_LEVEL;
    if (this->set_output_levels(this->pending_levels_) != m5_chain_base::ChainStatus::CHAIN_STATUS_OK) {
      this->pending_op_ = FlushOp::FLUSH_OP_NONE;
      return false;
    }
    return true;
  }

  for (uint8_t channel = 0; channel < SERVOS2_CHANNEL_COUNT; channel++) {
    auto &state = this->rgb_flush_[channel];
    if (!state.pending)
      continue;
    if (state.next < state.count) {
      if (state.next == 0)
        state.flushing_generation = state.generation;
      this->pending_op_ = FlushOp::FLUSH_OP_RGB_BUFFER;
      this->pending_channel_ = channel;
      const uint8_t *pixel = state.buffer + state.next * 3;
      if (this->set_rgb(state.offset + state.next, pixel[0], pixel[1], pixel[2]) !=
          m5_chain_base::ChainStatus::CHAIN_STATUS_OK) {
        this->pending_op_ = FlushOp::FLUSH_OP_NONE;
        return false;
      }
      return true;
    }
    this->pending_op_ = FlushOp::FLUSH_OP_RGB_LATCH;
    this->pending_channel_ = channel;
    if (this->set_rgb_config(channel, state.count, true) != m5_chain_base::ChainStatus::CHAIN_STATUS_OK) {
      this->pending_op_ = FlushOp::FLUSH_OP_NONE;
      return false;
    }
    return true;
  }
  return false;
}

void M5Unit8Servos2Chain::publish_levels_() {
  for (uint8_t channel = 0; channel < SERVOS2_CHANNEL_COUNT; channel++) {
    if (this->level_switches_[channel] != nullptr)
      this->level_switches_[channel]->publish_state(this->pending_levels_[channel]);
  }
}

void M5Unit8Servos2Chain::on_response(const m5_chain_base::ChainFrame &frame) {
  if (frame.command == SERVOS2_COMMAND_GET_MODE_ALL) {
    this->handle_modes_(frame);
    return;
  }
  const FlushOp operation = this->pending_op_;
  this->pending_op_ = FlushOp::FLUSH_OP_NONE;
  if (frame.data_size < 1 || frame.data[0] == 0) {
    this->status_set_warning();
    this->next_flush_at_ = App.get_loop_component_start_time() + SERVOS2_WRITE_BACKOFF_MS;
    return;
  }
  this->timeout_warning_active_ = false;
  this->status_clear_warning();

  switch (operation) {
    case FlushOp::FLUSH_OP_MODE:
      this->applied_modes_[this->pending_channel_] = this->pending_mode_;
      break;
    case FlushOp::FLUSH_OP_SERVO:
      if (this->servo_generation_ == this->sent_servo_generation_)
        this->servo_dirty_ = false;
      break;
    case FlushOp::FLUSH_OP_PWM:
      if (this->pwm_generation_ == this->sent_pwm_generation_)
        this->pwm_dirty_ = false;
      break;
    case FlushOp::FLUSH_OP_LEVEL:
      if (this->level_generation_ == this->sent_level_generation_)
        this->levels_dirty_ = false;
      this->publish_levels_();
      break;
    case FlushOp::FLUSH_OP_RGB_BUFFER:
      this->rgb_flush_[this->pending_channel_].next++;
      break;
    case FlushOp::FLUSH_OP_RGB_LATCH: {
      auto &state = this->rgb_flush_[this->pending_channel_];
      if (state.generation == state.flushing_generation)
        state.pending = false;
      state.next = 0;
      break;
    }
    default:
      break;
  }
}

void M5Unit8Servos2Chain::on_unsolicited(const m5_chain_base::ChainFrame &frame) {
  // A response can arrive after the request timeout when it traverses several chain nodes.
  if (frame.index == this->get_chain_index() && frame.command == SERVOS2_COMMAND_GET_MODE_ALL)
    this->handle_modes_(frame);
}

void M5Unit8Servos2Chain::handle_modes_(const m5_chain_base::ChainFrame &frame) {
  // This response carries the eight channel modes and no operation status byte.
  if (frame.data_size < SERVOS2_CHANNEL_COUNT) {
    this->status_set_warning();
    return;
  }
  for (uint8_t channel = 0; channel < SERVOS2_CHANNEL_COUNT; channel++)
    this->applied_modes_[channel] = frame.data[channel];
  ESP_LOGV(TAG, "8Servos2 channel modes: %u %u %u %u %u %u %u %u", frame.data[0], frame.data[1], frame.data[2],
           frame.data[3], frame.data[4], frame.data[5], frame.data[6], frame.data[7]);
  this->timeout_warning_active_ = false;
  this->status_clear_warning();
}

void M5Unit8Servos2Chain::on_request_timeout(uint8_t index, uint8_t command) {
  this->pending_op_ = FlushOp::FLUSH_OP_NONE;
  this->next_flush_at_ = App.get_loop_component_start_time() + SERVOS2_WRITE_BACKOFF_MS;
  if (!this->timeout_warning_active_) {
    ESP_LOGW(TAG, "Timeout waiting for Unit 8Servos2 Chain response (index=%u, command=0x%02X)", index, command);
    this->timeout_warning_active_ = true;
  }
  this->status_set_warning();
}

}  // namespace esphome::m5_unit_8servos2_chain
