#pragma once

#include "esphome/components/output/binary_output.h"
#include "esphome/components/output/float_output.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "../m5_unit_8servos2_chain.h"

namespace esphome::m5_unit_8servos2_chain {

class M5Unit8Servos2FloatOutput : public output::FloatOutput, public Component, public Parented<M5Unit8Servos2Chain> {
 public:
  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void set_output_type(Servos2OutputType output_type) { this->output_type_ = output_type; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  void write_state(float state) override;

  uint8_t channel_{0};
  Servos2OutputType output_type_{Servos2OutputType::SERVOS2_OUTPUT_TYPE_SERVO};
};

class M5Unit8Servos2BinaryOutput : public output::BinaryOutput, public Component, public Parented<M5Unit8Servos2Chain> {
 public:
  void set_channel(uint8_t channel) { this->channel_ = channel; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  void write_state(bool state) override;

  uint8_t channel_{0};
};

}  // namespace esphome::m5_unit_8servos2_chain
