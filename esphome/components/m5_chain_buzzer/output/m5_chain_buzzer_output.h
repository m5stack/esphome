#pragma once

#include "esphome/components/m5_chain_base/m5_chain_base.h"
#include "esphome/components/output/float_output.h"

#include "../m5_chain_buzzer.h"

namespace esphome::m5_chain_buzzer {

class M5ChainBuzzerOutput : public output::FloatOutput, public Parented<M5ChainBuzzer> {
 public:
  void write_state(float state) override;
  void update_frequency(float frequency) override;

 protected:
  uint16_t frequency_{2700};
};

}  // namespace esphome::m5_chain_buzzer
