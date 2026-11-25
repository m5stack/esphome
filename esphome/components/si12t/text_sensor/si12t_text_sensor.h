#pragma once

#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::si12t {

/// One touch channel of the Si12T, published by the hub that owns it
class SI12TTextSensor : public text_sensor::TextSensor {
 public:
  /// Zero based channel, as used by the output registers
  void set_channel(uint8_t channel) { this->channel_ = channel; }
  /// SENx nibble of this channel: ChxHL in bit 3, ChxM[2:0] in bits 2:0
  void set_sensitivity(uint8_t sensitivity) { this->sensitivity_ = sensitivity; }
  uint8_t get_sensitivity() const { return this->sensitivity_; }

  /// Publish a touch intensity level, unless it is the one already published
  void publish_level(uint8_t level);
  /// Log the entity and the channel configuration it was built with
  void dump_channel(const char *tag);

 protected:
  uint8_t channel_{0};
  uint8_t sensitivity_{0x0B};  // the reset value of a SENx nibble: ChxHL = 1, ChxM = 011
  uint8_t last_{0xFF};         // 0xFF so that the first level read is published
};

}  // namespace esphome::si12t
