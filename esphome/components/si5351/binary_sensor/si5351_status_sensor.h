#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

#include "../si5351.h"


namespace esphome::si5351 {

/// Mirrors one status bit of register 0 (loss of lock / loss of signal).
class Si5351StatusSensor : public binary_sensor::BinarySensor,
                            public PollingComponent,
                            public Parented<Si5351Component> {
 public:
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_status_bit(uint8_t bit) { this->status_bit_ = bit; }

 protected:
  uint8_t status_bit_{0};
};

} // namespace esphome::si5351

