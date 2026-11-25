#include "m5stack_ioe1_light.h"

#include "esphome/core/color.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::m5stack_ioe1 {

ESPHOME_LOG_TAG(TAG, "m5stack_ioe1.light");

void M5StackIOE1AddressableLight::setup() {
  RAMAllocator<uint8_t> allocator;
  if (!this->internal_accessor_.allocate(allocator, this->num_leds_)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_ALLOC_FAIL));
    return;
  }
  this->accessor_ = &this->internal_accessor_;

  // Enable the LEDs and refresh the on-device RAM
  this->parent_->setup_neopixel(this->num_leds_);
}

void M5StackIOE1AddressableLight::write_state(light::LightState * /*state*/) {
  uint8_t *dst = this->rgb565_buf_.data();
  for (uint8_t i = 0; i < this->num_leds_; i++) {
    const Color color = this->internal_accessor_.get_raw(i);
    const uint16_t rgb565 = ((color.red & 0xF8) << 8) | ((color.green & 0xFC) << 3) | (color.blue >> 3);
    *dst++ = rgb565 & 0xFF;
    *dst++ = rgb565 >> 8;
  }

  this->parent_->set_led_color(this->rgb565_buf_.data(), this->num_leds_ * 2);
  this->mark_shown_();
}

}  // namespace esphome::m5stack_ioe1
