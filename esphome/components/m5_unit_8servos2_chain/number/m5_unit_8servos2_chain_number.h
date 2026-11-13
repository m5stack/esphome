#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "../m5_unit_8servos2_chain.h"

namespace esphome::m5_unit_8servos2_chain {

class M5Unit8Servos2AngleNumber : public number::Number, public Component, public Parented<M5Unit8Servos2Chain> {
 public:
  void set_channel(uint8_t channel) { this->channel_ = channel; }

  void setup() override;
  void dump_config() override;

 protected:
  void control(float value) override;

  uint8_t channel_{0};
};

class M5Unit8Servos2DutyNumber : public number::Number, public Component, public Parented<M5Unit8Servos2Chain> {
 public:
  void set_channel(uint8_t channel) { this->channel_ = channel; }

  void setup() override;
  void dump_config() override;

 protected:
  void control(float value) override;

  uint8_t channel_{0};
};

}  // namespace esphome::m5_unit_8servos2_chain
