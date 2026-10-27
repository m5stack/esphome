#pragma once

#include <cstdint>

#include "esphome/components/m5_chain_base/m5_chain_base.h"
#include "esphome/components/output/float_output.h"

namespace esphome::m5_chain_base {

enum class LedChannel : uint8_t {
  LED_BRIGHTNESS = 0,
  RGB_BRIGHTNESS = 1,
  RGB_RED = 2,
  RGB_GREEN = 3,
  RGB_BLUE = 4,
};

class M5ChainLedOutput : public output::FloatOutput, public Parented<M5ChainBase> {
 public:
  void set_channel(LedChannel channel) { this->channel_ = channel; }
  void write_state(float state) override;

 protected:
  LedChannel channel_{LedChannel::LED_BRIGHTNESS};
};

}  // namespace esphome::m5_chain_base
