#include "m5_chain_dlight.h"

#include <cinttypes>
#include <type_traits>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::m5_chain_dlight {

ESPHOME_LOG_TAG(TAG, "m5_chain_dlight");

using m5_chain_base::ChainCommand;
using m5_chain_base::ChainFrame;
using m5_chain_base::ChainStatus;

static constexpr uint8_t cmd(DLightCommand command) { return static_cast<uint8_t>(command); }
static constexpr uint8_t cmd(ChainCommand command) { return static_cast<uint8_t>(command); }

static constexpr uint8_t OPERATION_SUCCESS = 1;
static constexpr uint16_t LUX_DATA_SIZE = 4;

/// Give an alarm its first state from a reading; once it has one, only the device's reports change it.
static void publish_initial_alarm(binary_sensor::BinarySensor *alarm, bool active) {
  if (alarm != nullptr && !alarm->has_state())
    alarm->publish_state(active);
}

/// Write `value` least significant byte first, as every multi-byte field of the protocol is.
template<typename T> static void put_le(uint8_t *data, T value) {
  for (size_t i = 0; i < sizeof(T); i++)
    data[i] = static_cast<uint8_t>(static_cast<std::make_unsigned_t<T>>(value) >> (8 * i));
}

void M5ChainDLight::setup() {
  // Always send the event mode: with no alarm binary sensor the reports are switched off.
  this->queue_job_(DLightJob::DLIGHT_JOB_EVENT_MODE);
  this->send_next_job_();
}

void M5ChainDLight::dump_config() {
  this->dump_base_config();
  ESP_LOGCONFIG(TAG,
                "M5Stack Chain DLight:\n"
                "  Chain index: %u",
                this->get_chain_index());
  if (this->gain_ != NOT_SET) {
    ESP_LOGCONFIG(TAG, "  Gain code: %u", this->gain_);
  }
  if (this->resolution_ != NOT_SET) {
    ESP_LOGCONFIG(TAG, "  Resolution code: %u", this->resolution_);
  }
  if (this->events_enabled_) {
    ESP_LOGCONFIG(TAG, "  Event thresholds: %" PRIu32 " to %" PRIu32 " lx", this->threshold_low_,
                  this->threshold_high_);
  }
  LOG_SENSOR("  ", "Illuminance", this->illuminance_sensor_);
  LOG_BINARY_SENSOR("  ", "Low light", this->low_light_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "High light", this->high_light_binary_sensor_);
}

void M5ChainDLight::set_gain(uint8_t gain) {
  this->gain_ = gain;
  this->queue_job_(DLightJob::DLIGHT_JOB_GAIN);
}

void M5ChainDLight::set_resolution(uint8_t resolution) {
  this->resolution_ = resolution;
  this->queue_job_(DLightJob::DLIGHT_JOB_RESOLUTION);
}

void M5ChainDLight::set_thresholds(uint32_t low, uint32_t high) {
  this->threshold_low_ = low;
  this->threshold_high_ = high;
  this->events_enabled_ = true;
  this->queue_job_(DLightJob::DLIGHT_JOB_THRESHOLD);
  this->queue_job_(DLightJob::DLIGHT_JOB_INTERRUPT_ENABLE);
}

ChainStatus M5ChainDLight::set_rgb_color(uint8_t red, uint8_t green, uint8_t blue) {
  this->rgb_red_ = red;
  this->rgb_green_ = green;
  this->rgb_blue_ = blue;
  this->queue_job_(DLightJob::DLIGHT_JOB_RGB);
  this->send_next_job_();
  return ChainStatus::CHAIN_STATUS_OK;
}

ChainStatus M5ChainDLight::set_rgb_color_component(uint8_t component, uint8_t value) {
  if (component > 2)
    return ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  uint8_t *channels[] = {&this->rgb_red_, &this->rgb_green_, &this->rgb_blue_};
  *channels[component] = value;
  this->queue_job_(DLightJob::DLIGHT_JOB_RGB);
  this->send_next_job_();
  return ChainStatus::CHAIN_STATUS_OK;
}

ChainStatus M5ChainDLight::set_led_brightness(uint8_t brightness) {
  if (brightness > 100)
    return ChainStatus::CHAIN_STATUS_PARAMETER_ERROR;
  this->brightness_ = brightness;
  this->queue_job_(DLightJob::DLIGHT_JOB_BRIGHTNESS);
  this->send_next_job_();
  return ChainStatus::CHAIN_STATUS_OK;
}

void M5ChainDLight::poll_next_() {
  if (this->illuminance_sensor_ != nullptr)
    this->queue_job_(DLightJob::DLIGHT_JOB_READ_LUX);
  this->send_next_job_();
}

void M5ChainDLight::send_next_job_() {
  if (this->pending_jobs_ == 0 || this->request_pending())
    return;

  DLightJob job = DLightJob::DLIGHT_JOB_COUNT;
  for (uint8_t i = 0; i < static_cast<uint8_t>(DLightJob::DLIGHT_JOB_COUNT); i++) {
    if (this->pending_jobs_ & (1u << i)) {
      job = static_cast<DLightJob>(i);
      break;
    }
  }

  uint8_t data[8];
  uint16_t size = 0;
  uint8_t command = 0;
  switch (job) {
    case DLightJob::DLIGHT_JOB_GAIN:
      command = cmd(DLightCommand::DLIGHT_COMMAND_SET_GAIN);
      data[0] = this->gain_;
      size = 1;
      break;
    case DLightJob::DLIGHT_JOB_RESOLUTION:
      command = cmd(DLightCommand::DLIGHT_COMMAND_SET_RESOLUTION);
      data[0] = this->resolution_;
      size = 1;
      break;
    case DLightJob::DLIGHT_JOB_THRESHOLD:
      // The device takes the high threshold first.
      command = cmd(DLightCommand::DLIGHT_COMMAND_SET_INTERRUPT_THRESHOLD);
      put_le<uint32_t>(data, this->threshold_high_);
      put_le<uint32_t>(data + 4, this->threshold_low_);
      size = 8;
      break;
    case DLightJob::DLIGHT_JOB_INTERRUPT_ENABLE:
      command = cmd(DLightCommand::DLIGHT_COMMAND_SET_INTERRUPT_ENABLE);
      data[0] = this->events_enabled_;
      size = 1;
      break;
    case DLightJob::DLIGHT_JOB_EVENT_MODE:
      command = cmd(DLightCommand::DLIGHT_COMMAND_SET_EVENT_MODE);
      data[0] = this->events_enabled_;
      size = 1;
      break;
    case DLightJob::DLIGHT_JOB_RGB:
      command = cmd(ChainCommand::CHAIN_COMMAND_SET_RGB_VALUE);
      data[0] = 0;  // first LED
      data[1] = 1;  // one LED
      data[2] = this->rgb_red_;
      data[3] = this->rgb_green_;
      data[4] = this->rgb_blue_;
      size = 5;
      break;
    case DLightJob::DLIGHT_JOB_BRIGHTNESS:
      command = cmd(ChainCommand::CHAIN_COMMAND_SET_RGB_LIGHT);
      data[0] = this->brightness_;
      data[1] = 0;  // do not save to flash
      size = 2;
      break;
    case DLightJob::DLIGHT_JOB_READ_LUX:
      command = cmd(DLightCommand::DLIGHT_COMMAND_GET_LUX);
      break;
    case DLightJob::DLIGHT_JOB_COUNT:
      return;
  }

  const auto status = this->request(this->get_chain_index(), command, data, size);
  if (status == ChainStatus::CHAIN_STATUS_BUSY)
    return;
  // Clear the job when it leaves, not when it is answered: a new value queued while this one is on the
  // wire must be sent again afterwards.
  this->pending_jobs_ &= ~(1u << static_cast<uint8_t>(job));
  if (status == ChainStatus::CHAIN_STATUS_OK) {
    this->active_job_ = job;
  } else {
    ESP_LOGW(TAG, "Could not queue command 0x%02X (status=%u)", command, static_cast<uint8_t>(status));
    this->status_set_warning();
  }
}

void M5ChainDLight::on_response(const ChainFrame &frame) {
  this->active_job_ = DLightJob::DLIGHT_JOB_COUNT;
  if (!this->handle_lux_(frame)) {
    // Every other request is a setting: its answer is one operation status byte.
    if (frame.data_size < 1 || frame.data[0] != OPERATION_SUCCESS) {
      ESP_LOGW(TAG, "Chain DLight rejected command 0x%02X", frame.command);
      this->status_set_warning();
    } else {
      this->mark_alive_();
    }
  }
  this->send_next_job_();
}

void M5ChainDLight::on_unsolicited(const ChainFrame &frame) {
  if (frame.index != this->get_chain_index())
    return;
  if (frame.command == cmd(DLightCommand::DLIGHT_COMMAND_EVENT_REPORT)) {
    this->handle_event_(frame);
    return;
  }
  // A reading can arrive after the request timed out when it crosses several chain nodes.
  this->handle_lux_(frame);
}

bool M5ChainDLight::handle_lux_(const ChainFrame &frame) {
  if (frame.command != cmd(DLightCommand::DLIGHT_COMMAND_GET_LUX))
    return false;
  if (frame.data_size < LUX_DATA_SIZE) {
    this->status_set_warning();
    return true;
  }
  const uint32_t lux = encode_value_le<uint32_t>(frame.data);
  if (this->illuminance_sensor_ != nullptr)
    this->illuminance_sensor_->publish_state(lux);
  publish_initial_alarm(this->low_light_binary_sensor_, lux < this->threshold_low_);
  publish_initial_alarm(this->high_light_binary_sensor_, lux > this->threshold_high_);
  this->mark_alive_();
  return true;
}

void M5ChainDLight::handle_event_(const ChainFrame &frame) {
  if (frame.data_size < 1) {
    ESP_LOGW(TAG, "Malformed Chain DLight event report");
    return;
  }
  const auto event = static_cast<DLightEvent>(frame.data[0]);
  ESP_LOGD(TAG, "Chain DLight event %u", frame.data[0]);
  switch (event) {
    case DLightEvent::DLIGHT_EVENT_LOW_ACTIVE:
    case DLightEvent::DLIGHT_EVENT_LOW_RELEASE:
      if (this->low_light_binary_sensor_ != nullptr)
        this->low_light_binary_sensor_->publish_state(event == DLightEvent::DLIGHT_EVENT_LOW_ACTIVE);
      break;
    case DLightEvent::DLIGHT_EVENT_HIGH_ACTIVE:
    case DLightEvent::DLIGHT_EVENT_HIGH_RELEASE:
      if (this->high_light_binary_sensor_ != nullptr)
        this->high_light_binary_sensor_->publish_state(event == DLightEvent::DLIGHT_EVENT_HIGH_ACTIVE);
      break;
    default:
      ESP_LOGW(TAG, "Unknown Chain DLight event %u", frame.data[0]);
      return;
  }
  this->mark_alive_();
}

void M5ChainDLight::mark_alive_() {
  this->timeout_warning_active_ = false;
  this->status_clear_warning();
}

void M5ChainDLight::on_request_timeout(uint8_t index, uint8_t command) {
  // Settings and LED values are kept and resent on the next poll; a missed reading is simply dropped.
  const DLightJob job = this->active_job_;
  this->active_job_ = DLightJob::DLIGHT_JOB_COUNT;
  if (job < DLightJob::DLIGHT_JOB_READ_LUX)
    this->queue_job_(job);
  if (!this->timeout_warning_active_) {
    ESP_LOGW(TAG, "Timeout waiting for Chain DLight response (index=%u, command=0x%02X)", index, command);
    this->timeout_warning_active_ = true;
  }
  this->status_set_warning();
}

}  // namespace esphome::m5_chain_dlight
