#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

#include "../m5stack_ioe1.h"

namespace esphome::m5stack_ioe1 {

class M5StackIOE1ADCSensor : public sensor::Sensor, public PollingComponent, public Parented<M5StackIOE1Component> {
 public:
  void update() override;
  void dump_config() override;

  void set_channel(M5StackIOE1ADCChannel channel) { this->channel_ = channel; }

 protected:
  M5StackIOE1ADCChannel channel_{ADC_NONE};
};

class M5StackIOE1TemperatureSensor : public sensor::Sensor,
                                     public PollingComponent,
                                     public Parented<M5StackIOE1Component> {
 public:
  void update() override;
};

}  // namespace esphome::m5stack_ioe1
