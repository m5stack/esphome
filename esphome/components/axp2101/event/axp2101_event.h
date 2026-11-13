#pragma once

#include "esphome/components/axp_base/axp_base.h"
#include "esphome/components/event/event.h"
#include "esphome/core/component.h"

namespace esphome::axp2101 {

/// Reports the chip's interrupt sources as ESPHome event types.
///
/// One entity carries every source it subscribes to, and the event type name is
/// the name from the model table, so `power_key_short_press` in the
/// configuration is also what Home Assistant reports.
class AXP2101Event : public event::Event, public Component, public Parented<axp_base::AxpComponent> {
 public:
  void setup() override;
  void dump_config() override;

 protected:
  /// Trigger one event per interrupt bit that this entity listens for.
  void on_events_(uint64_t mask);

  /// Interrupt bits this entity subscribed to, and therefore triggers.
  uint64_t event_mask_{0};
};

}  // namespace esphome::axp2101
