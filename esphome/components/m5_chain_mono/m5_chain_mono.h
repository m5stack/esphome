#pragma once

#include <array>
#include <cstdint>

#include "esphome/components/m5_chain_base/m5_chain_base.h"

namespace esphome::m5_chain_mono {

enum class MonoCommand : uint8_t {
  MONO_COMMAND_SET_DISPLAY_BUFFER = 0x31,
  MONO_COMMAND_SET_BRIGHTNESS = 0xE2,
};

/// The panel dims in eight steps: level 0 is dark, level 7 is the brightest.
static constexpr uint8_t MAX_SCREEN_BRIGHTNESS_LEVEL = 7;

class M5ChainMono : public m5_chain_base::M5ChainBase {
 public:
  void setup() override;
  void dump_config() override;
  m5_chain_base::ChainStatus set_display_buffer(const uint8_t *buffer, uint8_t size);
  /// Set the panel brightness level (0-7). The level is not written to the module's flash.
  m5_chain_base::ChainStatus set_screen_brightness(uint8_t level);

 protected:
  void on_response(const m5_chain_base::ChainFrame &frame) override;
  void on_request_timeout(uint8_t index, uint8_t command) override;

  m5_chain_base::ChainStatus send_pending_();
  std::array<uint8_t, 8> pending_buffer_{};
  bool pending_{false};
  uint8_t pending_brightness_{0};
  /// The level last queued for the panel, or 0xFF (no valid level) before the first one and after
  /// one was given up on, so the next write sends it again.
  uint8_t brightness_level_{0xFF};
  bool brightness_pending_{false};
};

}  // namespace esphome::m5_chain_mono
