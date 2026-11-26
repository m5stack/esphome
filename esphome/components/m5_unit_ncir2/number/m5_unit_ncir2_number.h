#pragma once

#include "esphome/components/m5_unit_ncir2/m5_unit_ncir2.h"
#include "esphome/components/number/number.h"
#include "esphome/core/helpers.h"

namespace esphome::m5_unit_ncir2 {

class M5UnitNCIR2Number : public number::Number, public Parented<M5UnitNCIR2> {
 public:
  explicit M5UnitNCIR2Number(Setting setting) : setting_(setting) {}

 protected:
  void control(float value) override {
    if (this->parent_->control_setting(this->setting_, value))
      this->publish_state(value);
  }

  const Setting setting_;
};

}  // namespace esphome::m5_unit_ncir2
