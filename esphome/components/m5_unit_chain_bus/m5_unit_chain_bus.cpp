#include "m5_unit_chain_bus.h"

#include <algorithm>
#include <array>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::m5_unit_chain_bus {

ESPHOME_LOG_TAG(TAG, "m5_unit_chain_bus");

static constexpr uint8_t CHAIN_BUS_COMMAND_I2C_INIT = static_cast<uint8_t>(ChainBusCommand::CHAIN_BUS_COMMAND_I2C_INIT);
static constexpr uint8_t CHAIN_BUS_COMMAND_I2C_READ = static_cast<uint8_t>(ChainBusCommand::CHAIN_BUS_COMMAND_I2C_READ);
static constexpr uint8_t CHAIN_BUS_COMMAND_I2C_WRITE =
    static_cast<uint8_t>(ChainBusCommand::CHAIN_BUS_COMMAND_I2C_WRITE);
static constexpr uint8_t CHAIN_BUS_COMMAND_I2C_MEM_READ =
    static_cast<uint8_t>(ChainBusCommand::CHAIN_BUS_COMMAND_I2C_MEM_READ);
static constexpr uint8_t CHAIN_BUS_COMMAND_I2C_MEM_WRITE =
    static_cast<uint8_t>(ChainBusCommand::CHAIN_BUS_COMMAND_I2C_MEM_WRITE);
static constexpr uint8_t CHAIN_BUS_COMMAND_I2C_SCAN_ADDRESS =
    static_cast<uint8_t>(ChainBusCommand::CHAIN_BUS_COMMAND_I2C_SCAN_ADDRESS);
static constexpr uint8_t CHAIN_BUS_COMMAND_SET_OUTPUT_MODE =
    static_cast<uint8_t>(ChainBusCommand::CHAIN_BUS_COMMAND_SET_OUTPUT_MODE);
static constexpr uint8_t CHAIN_BUS_COMMAND_SET_OUTPUT_LEVEL =
    static_cast<uint8_t>(ChainBusCommand::CHAIN_BUS_COMMAND_SET_OUTPUT_LEVEL);
static constexpr uint8_t CHAIN_BUS_COMMAND_GET_OUTPUT_LEVEL =
    static_cast<uint8_t>(ChainBusCommand::CHAIN_BUS_COMMAND_GET_OUTPUT_LEVEL);
static constexpr uint8_t CHAIN_BUS_COMMAND_SET_INPUT_MODE =
    static_cast<uint8_t>(ChainBusCommand::CHAIN_BUS_COMMAND_SET_INPUT_MODE);
static constexpr uint8_t CHAIN_BUS_COMMAND_GET_INPUT_LEVEL =
    static_cast<uint8_t>(ChainBusCommand::CHAIN_BUS_COMMAND_GET_INPUT_LEVEL);
static constexpr uint8_t CHAIN_BUS_COMMAND_SET_NVIC_MODE =
    static_cast<uint8_t>(ChainBusCommand::CHAIN_BUS_COMMAND_SET_NVIC_MODE);
static constexpr uint8_t CHAIN_BUS_COMMAND_SET_ADC_MODE =
    static_cast<uint8_t>(ChainBusCommand::CHAIN_BUS_COMMAND_SET_ADC_MODE);
static constexpr uint8_t CHAIN_BUS_COMMAND_GET_ADC_VALUE =
    static_cast<uint8_t>(ChainBusCommand::CHAIN_BUS_COMMAND_GET_ADC_VALUE);
static constexpr uint8_t CHAIN_BUS_COMMAND_GET_WORK_STATUS =
    static_cast<uint8_t>(ChainBusCommand::CHAIN_BUS_COMMAND_GET_WORK_STATUS);
static constexpr uint8_t CHAIN_BUS_COMMAND_NVIC_TRIGGER_REPORT =
    static_cast<uint8_t>(ChainBusCommand::CHAIN_BUS_COMMAND_NVIC_TRIGGER_REPORT);

/// The two Grove pins the unit can drive.
static constexpr uint8_t CHAIN_BUS_PIN_1 = 1;
static constexpr uint8_t CHAIN_BUS_PIN_2 = 2;
/// External interrupt report: the trigger mode sits in the first data byte.
static constexpr uint8_t CHAIN_BUS_TRIGGER_RISING_EDGE = 0x00;

void M5UnitChainBus::setup() {
  ESP_LOGCONFIG(TAG, "Setting up M5Stack Unit ChainBus (chain_index=%u)...", this->get_chain_index());
}

void M5UnitChainBus::dump_config() {
  this->dump_base_config();
  ESP_LOGCONFIG(TAG, "M5Stack Unit ChainBus:");
  ESP_LOGCONFIG(TAG, "  Chain index: %u", this->get_chain_index());
}

void M5UnitChainBus::poll_next_() {
  if (this->request_pending())
    return;
  if (this->request(this->get_chain_index(), CHAIN_BUS_COMMAND_GET_WORK_STATUS) !=
      m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
    this->status_set_warning();
}

m5_chain_base::ChainStatus M5UnitChainBus::i2c_init(ChainBusI2CSpeed speed) {
  const uint8_t data[] = {static_cast<uint8_t>(speed)};
  return this->request(this->get_chain_index(), CHAIN_BUS_COMMAND_I2C_INIT, data, sizeof(data));
}

m5_chain_base::ChainStatus M5UnitChainBus::i2c_write(uint8_t address, std::initializer_list<uint8_t> data) {
  if (data.size() == 0 || data.size() > CHAIN_BUS_I2C_MAX_LENGTH)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  std::array<uint8_t, 2 + CHAIN_BUS_I2C_MAX_LENGTH> payload{};
  payload[0] = address;
  payload[1] = static_cast<uint8_t>(data.size());
  std::copy(data.begin(), data.end(), payload.begin() + 2);
  return this->request(this->get_chain_index(), CHAIN_BUS_COMMAND_I2C_WRITE, payload.data(),
                       static_cast<uint16_t>(data.size() + 2));
}

m5_chain_base::ChainStatus M5UnitChainBus::i2c_read(uint8_t address, uint8_t length) {
  if (length == 0 || length > CHAIN_BUS_I2C_MAX_LENGTH)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {address, length};
  const auto status = this->request(this->get_chain_index(), CHAIN_BUS_COMMAND_I2C_READ, data, sizeof(data));
  if (status == m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
    this->read_address_ = address;
  return status;
}

m5_chain_base::ChainStatus M5UnitChainBus::i2c_mem_read(uint8_t address, uint16_t reg, ChainBusRegisterSize reg_size,
                                                        uint8_t length) {
  if (length == 0 || length > CHAIN_BUS_I2C_MAX_LENGTH)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {address, static_cast<uint8_t>(reg_size), static_cast<uint8_t>(reg & 0xFF),
                          static_cast<uint8_t>(reg >> 8), length};
  const auto status = this->request(this->get_chain_index(), CHAIN_BUS_COMMAND_I2C_MEM_READ, data, sizeof(data));
  if (status == m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
    this->read_address_ = address;
  return status;
}

m5_chain_base::ChainStatus M5UnitChainBus::i2c_mem_write(uint8_t address, uint16_t reg, ChainBusRegisterSize reg_size,
                                                         std::initializer_list<uint8_t> data) {
  if (data.size() == 0 || data.size() > CHAIN_BUS_I2C_MAX_LENGTH)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  std::array<uint8_t, 5 + CHAIN_BUS_I2C_MAX_LENGTH> payload{};
  payload[0] = address;
  payload[1] = static_cast<uint8_t>(reg_size);
  payload[2] = static_cast<uint8_t>(reg & 0xFF);
  payload[3] = static_cast<uint8_t>(reg >> 8);
  payload[4] = static_cast<uint8_t>(data.size());
  std::copy(data.begin(), data.end(), payload.begin() + 5);
  return this->request(this->get_chain_index(), CHAIN_BUS_COMMAND_I2C_MEM_WRITE, payload.data(),
                       static_cast<uint16_t>(data.size() + 5));
}

m5_chain_base::ChainStatus M5UnitChainBus::i2c_scan() {
  return this->request(this->get_chain_index(), CHAIN_BUS_COMMAND_I2C_SCAN_ADDRESS);
}

m5_chain_base::ChainStatus M5UnitChainBus::set_output_mode(uint8_t pin, ChainBusOutputMode mode, ChainBusPull pull) {
  if (pin != CHAIN_BUS_PIN_1 && pin != CHAIN_BUS_PIN_2)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {pin, static_cast<uint8_t>(mode), static_cast<uint8_t>(pull)};
  return this->request(this->get_chain_index(), CHAIN_BUS_COMMAND_SET_OUTPUT_MODE, data, sizeof(data));
}

m5_chain_base::ChainStatus M5UnitChainBus::set_output_level(uint8_t pin, bool level) {
  if (pin != CHAIN_BUS_PIN_1 && pin != CHAIN_BUS_PIN_2)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {pin, static_cast<uint8_t>(level)};
  return this->request(this->get_chain_index(), CHAIN_BUS_COMMAND_SET_OUTPUT_LEVEL, data, sizeof(data));
}

m5_chain_base::ChainStatus M5UnitChainBus::get_output_level(uint8_t pin) {
  if (pin != CHAIN_BUS_PIN_1 && pin != CHAIN_BUS_PIN_2)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {pin};
  const auto status = this->request(this->get_chain_index(), CHAIN_BUS_COMMAND_GET_OUTPUT_LEVEL, data, sizeof(data));
  if (status == m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
    this->read_pin_ = pin;
  return status;
}

m5_chain_base::ChainStatus M5UnitChainBus::set_input_mode(uint8_t pin, ChainBusPull pull) {
  if (pin != CHAIN_BUS_PIN_1 && pin != CHAIN_BUS_PIN_2)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {pin, static_cast<uint8_t>(pull)};
  return this->request(this->get_chain_index(), CHAIN_BUS_COMMAND_SET_INPUT_MODE, data, sizeof(data));
}

m5_chain_base::ChainStatus M5UnitChainBus::get_input_level(uint8_t pin) {
  if (pin != CHAIN_BUS_PIN_1 && pin != CHAIN_BUS_PIN_2)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {pin};
  const auto status = this->request(this->get_chain_index(), CHAIN_BUS_COMMAND_GET_INPUT_LEVEL, data, sizeof(data));
  if (status == m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
    this->read_pin_ = pin;
  return status;
}

m5_chain_base::ChainStatus M5UnitChainBus::set_nvic_mode(uint8_t pin, ChainBusPull pull, ChainBusTrigger trigger) {
  if (pin != CHAIN_BUS_PIN_1 && pin != CHAIN_BUS_PIN_2)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {pin, static_cast<uint8_t>(pull), static_cast<uint8_t>(trigger)};
  return this->request(this->get_chain_index(), CHAIN_BUS_COMMAND_SET_NVIC_MODE, data, sizeof(data));
}

m5_chain_base::ChainStatus M5UnitChainBus::set_adc_mode(uint8_t pin) {
  if (pin != CHAIN_BUS_PIN_1 && pin != CHAIN_BUS_PIN_2)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {pin};
  return this->request(this->get_chain_index(), CHAIN_BUS_COMMAND_SET_ADC_MODE, data, sizeof(data));
}

m5_chain_base::ChainStatus M5UnitChainBus::get_adc_value(uint8_t pin) {
  if (pin != CHAIN_BUS_PIN_1 && pin != CHAIN_BUS_PIN_2)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {pin};
  const auto status = this->request(this->get_chain_index(), CHAIN_BUS_COMMAND_GET_ADC_VALUE, data, sizeof(data));
  if (status == m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
    this->read_pin_ = pin;
  return status;
}

m5_chain_base::ChainStatus M5UnitChainBus::set_rgb_color(uint8_t red, uint8_t green, uint8_t blue) {
  this->rgb_red_ = red;
  this->rgb_green_ = green;
  this->rgb_blue_ = blue;
  this->rgb_update_pending_ = true;
  return this->send_pending_rgb_();
}

m5_chain_base::ChainStatus M5UnitChainBus::set_rgb_color_component(uint8_t component, uint8_t value) {
  if (component > 2)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  if (component == 0)
    this->rgb_red_ = value;
  else if (component == 1)
    this->rgb_green_ = value;
  else
    this->rgb_blue_ = value;
  this->rgb_update_pending_ = true;
  return this->send_pending_rgb_();
}

m5_chain_base::ChainStatus M5UnitChainBus::send_pending_rgb_() {
  if (!this->rgb_update_pending_ || this->request_pending())
    return m5_chain_base::ChainStatus::CHAIN_STATUS_OK;

  const uint8_t data[] = {0, 1, this->rgb_red_, this->rgb_green_, this->rgb_blue_};
  const auto status = this->request(this->get_chain_index(), m5_chain_base::ChainCommand::CHAIN_COMMAND_SET_RGB_VALUE,
                                    data, sizeof(data));
  if (status == m5_chain_base::ChainStatus::CHAIN_STATUS_OK)
    this->rgb_update_pending_ = false;
  return status;
}

m5_chain_base::ChainStatus M5UnitChainBus::set_led_brightness(uint8_t brightness) {
  if (brightness > 100)
    return m5_chain_base::ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  const uint8_t data[] = {brightness, 0};
  return this->request(this->get_chain_index(), m5_chain_base::ChainCommand::CHAIN_COMMAND_SET_RGB_LIGHT, data,
                       sizeof(data));
}

bool M5UnitChainBus::operation_succeeded_(const m5_chain_base::ChainFrame &frame) {
  if (frame.data_size < 1 || frame.data[0] == 0) {
    this->status_set_warning();
    return false;
  }
  this->timeout_warning_active_ = false;
  this->status_clear_warning();
  return true;
}

void M5UnitChainBus::on_response(const m5_chain_base::ChainFrame &frame) {
  if (frame.command == static_cast<uint8_t>(m5_chain_base::ChainCommand::CHAIN_COMMAND_SET_RGB_VALUE) ||
      frame.command == static_cast<uint8_t>(m5_chain_base::ChainCommand::CHAIN_COMMAND_SET_RGB_LIGHT)) {
    if (frame.data_size < 1 || frame.data[0] == 0) {
      this->status_set_warning();
      this->rgb_update_pending_ = false;
    } else {
      this->timeout_warning_active_ = false;
      this->status_clear_warning();
      this->send_pending_rgb_();
    }
    return;
  }

  switch (static_cast<ChainBusCommand>(frame.command)) {
    case ChainBusCommand::CHAIN_BUS_COMMAND_GET_WORK_STATUS:
      this->handle_work_status_(frame);
      return;
    case ChainBusCommand::CHAIN_BUS_COMMAND_I2C_READ:
    case ChainBusCommand::CHAIN_BUS_COMMAND_I2C_MEM_READ:
      this->handle_i2c_read_(frame);
      break;
    case ChainBusCommand::CHAIN_BUS_COMMAND_I2C_SCAN_ADDRESS:
      this->handle_i2c_scan_(frame);
      break;
    case ChainBusCommand::CHAIN_BUS_COMMAND_GET_OUTPUT_LEVEL:
    case ChainBusCommand::CHAIN_BUS_COMMAND_GET_INPUT_LEVEL:
      this->handle_pin_level_(frame);
      break;
    case ChainBusCommand::CHAIN_BUS_COMMAND_GET_ADC_VALUE:
      this->handle_adc_value_(frame);
      break;
    default:
      this->operation_succeeded_(frame);
      break;
  }
  this->send_pending_rgb_();
}

void M5UnitChainBus::on_unsolicited(const m5_chain_base::ChainFrame &frame) {
  if (frame.index != this->get_chain_index())
    return;
  if (frame.command == CHAIN_BUS_COMMAND_NVIC_TRIGGER_REPORT) {
    if (frame.data_size < 2) {
      this->status_set_warning();
      return;
    }
    // The report names the edge that fired, so it also carries the new pin level.
    const uint8_t trigger_mode = frame.data[0];
    const uint8_t pin = frame.data[1];
    const bool level = trigger_mode == CHAIN_BUS_TRIGGER_RISING_EDGE;
    ESP_LOGD(TAG, "ChainBus pin %u edge detected (%s)", pin,
             level ? LOG_STR_LITERAL("rising") : LOG_STR_LITERAL("falling"));
    this->pin_level_callback_.call(pin, level);
    this->timeout_warning_active_ = false;
    this->status_clear_warning();
    return;
  }
  if (frame.command == CHAIN_BUS_COMMAND_GET_WORK_STATUS) {
    // A late work status response is harmless; treat it like an on-time one.
    this->handle_work_status_(frame);
    return;
  }
}

void M5UnitChainBus::handle_i2c_read_(const m5_chain_base::ChainFrame &frame) {
  if (!this->operation_succeeded_(frame))
    return;
  const uint8_t payload_size = frame.data_size - 1;
  ESP_LOGD(TAG, "I2C read from 0x%02X returned %u byte(s)", this->read_address_, payload_size);
  // Reads are user initiated, so the copy handed to the callback is not a hot path.
  this->i2c_read_callback_.call(this->read_address_,
                                std::vector<uint8_t>(frame.data + 1, frame.data + frame.data_size));
}

void M5UnitChainBus::handle_i2c_scan_(const m5_chain_base::ChainFrame &frame) {
  if (!this->operation_succeeded_(frame))
    return;
  if (frame.data_size < 2) {
    this->status_set_warning();
    return;
  }
  const uint8_t count = std::min<uint8_t>(frame.data[1], frame.data_size - 2);
  for (uint8_t i = 0; i < count; i++) {
    ESP_LOGD(TAG, "I2C address %u on the bus: 0x%02X", i + 1, frame.data[2 + i]);
  }
  this->i2c_scan_callback_.call(std::vector<uint8_t>(frame.data + 2, frame.data + 2 + count));
}

void M5UnitChainBus::handle_pin_level_(const m5_chain_base::ChainFrame &frame) {
  if (!this->operation_succeeded_(frame))
    return;
  if (frame.data_size < 2) {
    this->status_set_warning();
    return;
  }
  const bool level = frame.data[1] != 0;
  ESP_LOGD(TAG, "ChainBus pin %u level is %s", this->read_pin_,
           level ? LOG_STR_LITERAL("high") : LOG_STR_LITERAL("low"));
  this->pin_level_callback_.call(this->read_pin_, level);
}

void M5UnitChainBus::handle_adc_value_(const m5_chain_base::ChainFrame &frame) {
  if (!this->operation_succeeded_(frame))
    return;
  if (frame.data_size < 3) {
    this->status_set_warning();
    return;
  }
  const uint16_t value = encode_uint16(frame.data[2], frame.data[1]);
  ESP_LOGD(TAG, "ChainBus pin %u ADC value is %u", this->read_pin_, value);
  this->adc_value_callback_.call(this->read_pin_, value);
}

void M5UnitChainBus::handle_work_status_(const m5_chain_base::ChainFrame &frame) {
  // This response carries the two pin modes and no operation status byte.
  if (frame.data_size < 2) {
    this->status_set_warning();
    return;
  }
  ESP_LOGV(TAG, "ChainBus pin modes: pin 1 = %u, pin 2 = %u", frame.data[0], frame.data[1]);
  this->timeout_warning_active_ = false;
  this->status_clear_warning();
}

void M5UnitChainBus::on_request_timeout(uint8_t index, uint8_t command) {
  if (!this->timeout_warning_active_) {
    ESP_LOGW(TAG, "Timeout waiting for Unit ChainBus response (index=%u, command=0x%02X)", index, command);
    this->timeout_warning_active_ = true;
  }
  this->status_set_warning();
}

}  // namespace esphome::m5_unit_chain_bus
