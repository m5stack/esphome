#pragma once

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#include "../aw88298.h"

namespace esphome::aw88298 {

/// Drives one of the amplifier's enum-like settings, such as the boost mode.
///
/// The setting value is the option index, the order the configuration lists the options in.
class AW88298Select : public select::Select, public Component, public Parented<AW88298> {
 public:
  void set_setting(Setting setting) { this->setting_ = setting; }

  void setup() override;
  void dump_config() override;

 protected:
  void control(size_t index) override;

  Setting setting_{Setting::SETTING_BOOST_MODE};
};

}  // namespace esphome::aw88298
