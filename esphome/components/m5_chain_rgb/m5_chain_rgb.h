#pragma once

#include <array>
#include <cstdint>

#include "esphome/components/m5_chain_base/m5_chain_base.h"

namespace esphome::m5_chain_rgb {

enum class RgbCommand : uint8_t {
  RGB_COMMAND_SET_DISPLAY_BUFFER = 0x31,
};

class M5ChainRGB : public m5_chain_base::M5ChainBase {
 public:
  void setup() override;
  void dump_config() override;
  m5_chain_base::ChainStatus set_display_buffer(const uint8_t *buffer, uint16_t size);

 protected:
  void on_response(const m5_chain_base::ChainFrame &frame) override;
  void on_request_timeout(uint8_t index, uint8_t command) override;

  m5_chain_base::ChainStatus send_pending_();
  std::array<uint8_t, 128> pending_buffer_{};
  bool pending_{false};
};

}  // namespace esphome::m5_chain_rgb
