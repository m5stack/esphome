#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#include "../aw88298.h"

namespace esphome::aw88298 {

/// Drives one of the amplifier's numeric settings, such as the boost voltage.
///
/// The register holds a quantised value, so after a change the entity publishes what the amplifier
/// was actually set to rather than what was asked for.
class AW88298Number : public number::Number, public Component, public Parented<AW88298> {
 public:
  void set_setting(Setting setting) { this->setting_ = setting; }

  void setup() override;
  void dump_config() override;

 protected:
  void control(float value) override;

  Setting setting_{Setting::SETTING_BOOST_VOLTAGE};
};

}  // namespace esphome::aw88298
