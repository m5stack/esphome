#pragma once

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#include "../aw88298.h"

namespace esphome::aw88298 {

/// Switches one of the amplifier's on/off settings, such as HDCC or the hardware AGC.
///
/// The register keeps its value across a reboot, so unless a restore mode is configured the
/// switch only reports what the amplifier is set to.
class AW88298Switch : public switch_::Switch, public Component, public Parented<AW88298> {
 public:
  void set_setting(Setting setting) { this->setting_ = setting; }

  void setup() override;
  void write_state(bool state) override;
  void dump_config() override;

 protected:
  Setting setting_{Setting::SETTING_HDCC};
};

}  // namespace esphome::aw88298
