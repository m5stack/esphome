#pragma once

#include "esphome/components/axp_base/axp_base.h"

namespace esphome::axp2101 {

/// X-Powers AXP2101.
///
/// All chip specific behaviour lives in the model table in axp2101.cpp; the
/// rails, ADC channels, charger settings and interrupt sources are addressed
/// through the shared axp_base API.
class AXP2101 : public axp_base::AxpComponent {
 public:
  AXP2101();
};

}  // namespace esphome::axp2101
