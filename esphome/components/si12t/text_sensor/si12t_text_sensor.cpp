#include "si12t_text_sensor.h"

#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::si12t {

// Datasheet 12.2.7: the two bits of a channel in the output registers
PROGMEM_STRING_TABLE(LevelStrings, "No touch", "Low", "Medium", "High");

void SI12TTextSensor::publish_level(uint8_t level) {
  if (level == this->last_)
    return;
  this->last_ = level;
  this->publish_state(LevelStrings::get_progmem_str(level, 0));
}

void SI12TTextSensor::dump_channel(const char *tag) {
  text_sensor::log_text_sensor(tag, "  ", LOG_STR_LITERAL("Si12T Touch"), this);
  ESP_LOGCONFIG(tag, "  Channel %u: sensitivity %u, %s sensitivity table", this->channel_ + 1U,
                this->sensitivity_ & 0x07,
                (this->sensitivity_ & 0x08) ? LOG_STR_LITERAL("high") : LOG_STR_LITERAL("low"));
}

}  // namespace esphome::si12t
