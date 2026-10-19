#pragma once

#include "esphome/components/output/float_output.h"
#include "esphome/core/log.h"
#include "../unit_scroll_sensor.h"

namespace esphome {
namespace unit_scroll {

static const char *const TAG_OUTPUT = "unit_scroll.output";

// Output channel enumeration
enum OutputChannel {
  RGB_RED = 0,
  RGB_GREEN = 1,
  RGB_BLUE = 2,
};

class UnitScrollOutput : public output::FloatOutput, public Component {
 public:
  void set_parent(UnitScrollComponent *parent) { parent_ = parent; }
  void set_channel(OutputChannel channel) { channel_ = channel; }

  void write_state(float state) override {
    if (parent_ == nullptr) {
      ESP_LOGW(TAG_OUTPUT, "Parent is null!");
      return;
    }

    uint8_t value = static_cast<uint8_t>(state * 255.0f);

    uint8_t r, g, b;
    if (!parent_->get_rgb(&r, &g, &b)) {
      ESP_LOGW(TAG_OUTPUT, "Failed to get current RGB, using defaults");
      r = g = b = 0;
    }

    if (channel_ == RGB_RED) {
      r = value;
    } else if (channel_ == RGB_GREEN) {
      g = value;
    } else if (channel_ == RGB_BLUE) {
      b = value;
    }

    ESP_LOGD(TAG_OUTPUT, "Setting RGB color: R=%d, G=%d, B=%d", r, g, b);

    if (!parent_->set_rgb(r, g, b)) {
      ESP_LOGW(TAG_OUTPUT, "Failed to set RGB color");
    }
  }

 protected:
  UnitScrollComponent *parent_{nullptr};
  OutputChannel channel_{RGB_RED};
};

}  // namespace unit_scroll
}  // namespace esphome
