#pragma once

#include <array>
#include <cstdint>

#include "esphome/components/m5_chain_base/m5_chain_base.h"

namespace esphome::m5_chain_rgb {

enum class RgbCommand : uint8_t {
  RGB_COMMAND_SET_DISPLAY_BUFFER = 0x31,
  RGB_COMMAND_SET_BRIGHTNESS = 0xE2,
};

/// The module dims every frame by its own brightness, given as a percentage.
static constexpr uint8_t MAX_SCREEN_BRIGHTNESS = 100;

class M5ChainRGB : public m5_chain_base::M5ChainBase {
 public:
  void setup() override;
  void dump_config() override;
  m5_chain_base::ChainStatus set_display_buffer(const uint8_t *buffer, uint16_t size);
  /// Set the module's own brightness in percent (0-100). The value is not written to the module's flash.
  m5_chain_base::ChainStatus set_screen_brightness(uint8_t percent);

 protected:
  void on_response(const m5_chain_base::ChainFrame &frame) override;
  void on_request_timeout(uint8_t index, uint8_t command) override;

  m5_chain_base::ChainStatus send_pending_();
  std::array<uint8_t, 128> pending_buffer_{};
  bool pending_{false};
  uint8_t pending_brightness_{0};
  /// The percent last queued for the module, or 0xFF (no valid value) before the first one and after
  /// one was given up on, so the next write sends it again.
  uint8_t brightness_percent_{0xFF};
  bool brightness_pending_{false};
};

}  // namespace esphome::m5_chain_rgb
