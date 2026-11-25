#include "m5stack_ioe1_sensor.h"

#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::m5stack_ioe1 {

ESPHOME_LOG_TAG(TAG, "m5stack_ioe1.sensor");

PROGMEM_STRING_TABLE(ADCChannelStrings, "ADC Disabled", "ADC 1", "ADC 2", "ADC 3", "ADC 4", "Reference Voltage");

static const LogString *adc_channel_to_string(M5StackIOE1ADCChannel channel) {
  return ADCChannelStrings::get_log_str(static_cast<uint8_t>(channel), 0);
}

void M5StackIOE1ADCSensor::update() { this->publish_state(this->parent_->get_adc_value(this->channel_)); }

void M5StackIOE1ADCSensor::dump_config() {
  ESP_LOGCONFIG(TAG,
                "M5STACK_IOE1 ADC Sensor:\n"
                "  ADC Channel: %s",
                LOG_STR_ARG(adc_channel_to_string(this->channel_)));
  LOG_SENSOR("  ", "ADC Sensor", this);
}

void M5StackIOE1TemperatureSensor::update() { this->publish_state(this->parent_->get_temperature_value()); }

}  // namespace esphome::m5stack_ioe1
