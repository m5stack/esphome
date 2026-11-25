#include "m5stack_ioe1_float_output.h"

#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::m5stack_ioe1 {

ESPHOME_LOG_TAG(TAG, "m5stack_ioe1.output");

PROGMEM_STRING_TABLE(PWMChannelStrings, "PWM_1", "PWM_2", "PWM_3", "PWM_4", "Unknown");
PROGMEM_STRING_TABLE(PWMPolarityStrings, "ACTIVE_HIGH", "ACTIVE_LOW");

static const LogString *pwm_channel_to_string(M5StackIOE1PWMChannel channel) {
  return PWMChannelStrings::get_log_str(static_cast<uint8_t>(channel), PWMChannelStrings::LAST_INDEX);
}

static const LogString *pwm_polarity_to_string(M5StackIOE1PWMPolarity polarity) {
  return PWMPolarityStrings::get_log_str(static_cast<uint8_t>(polarity), 0);
}

void M5StackIOE1FloatOutput::setup() { this->parent_->setup_pwm_channel(this->channel_, this->polarity_); }

void M5StackIOE1FloatOutput::write_state(float state) { this->parent_->set_pwm_value(this->channel_, state); }

void M5StackIOE1FloatOutput::dump_config() {
  ESP_LOGCONFIG(TAG,
                "M5STACK_IOE1 Float Output:\n"
                "  PWM Channel: %s\n"
                "  PWM Polarity: %s",
                LOG_STR_ARG(pwm_channel_to_string(this->channel_)),
                LOG_STR_ARG(pwm_polarity_to_string(this->polarity_)));
  LOG_FLOAT_OUTPUT(this);
}

}  // namespace esphome::m5stack_ioe1
