#pragma once

#include "esphome/core/component.h"

#ifdef USE_LVGL
#include "esphome/components/lvgl/lvgl_esphome.h"
#endif

namespace esphome::m5_sticks3_ui {

class M5Sticks3UI : public Component {
 public:
  void setup() override;
  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::PROCESSOR - 1.0f; }
};

}  // namespace esphome::m5_sticks3_ui
