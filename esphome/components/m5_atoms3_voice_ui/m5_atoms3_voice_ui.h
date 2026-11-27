#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "esphome/components/display/display.h"
#include "esphome/core/component.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Opaque esp_emote_gfx types; gfx.h is only needed by the implementation so the
// generated esphome.h stays free of the graphics headers.
typedef void *gfx_handle_t;
typedef struct gfx_disp gfx_disp_t;
typedef struct gfx_obj gfx_obj_t;

namespace esphome::voice_assistant {
class VoiceAssistant;
}

namespace esphome::m5_atoms3_voice_ui {

class M5Atoms3VoiceUI final : public Component {
 public:
  M5Atoms3VoiceUI(display::Display *display, const int *phase, voice_assistant::VoiceAssistant *assistant)
      : display_(display), phase_source_(phase), voice_assistant_(assistant) {}

  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  float get_setup_priority() const override { return setup_priority::PROCESSOR - 10; }

  void set_background_color(uint32_t color) { this->background_color_ = color; }
  void set_big_endian(bool big_endian) { this->big_endian_ = big_endian; }
  void set_frame_rate(uint32_t fps) { this->fps_ = fps; }
  void set_phase_mapping(size_t index, int phase) { this->phase_mapping_[index] = phase; }
  void set_animation_source(size_t index, const uint32_t *data, size_t size);

 protected:
  static void flush_cb_(gfx_disp_t *disp, int x1, int y1, int x2, int y2, const void *data);
  static void phase_task_(void *arg);
  void flush_(int x1, int y1, int x2, int y2, const void *data);
  void update_phase_();
  bool render_phase_(int phase);

  display::Display *display_;
  const int *phase_source_;
  voice_assistant::VoiceAssistant *voice_assistant_;
  std::array<int, 8> phase_mapping_{1, 2, 3, 4, 10, 11, 12, 20};
  std::array<const uint8_t *, 8> animation_data_{};
  std::array<size_t, 8> animation_size_{};
  int last_phase_{-1};
  int active_animation_{-1};
  uint32_t background_color_{0x10131A};
  uint32_t fps_{30};
  bool big_endian_{true};

  gfx_handle_t gfx_{nullptr};
  gfx_disp_t *disp_{nullptr};
  gfx_obj_t *anim_{nullptr};
  TaskHandle_t phase_task_handle_{nullptr};
  volatile bool phase_task_running_{false};
  volatile bool phase_task_done_{true};
};

}  // namespace esphome::m5_atoms3_voice_ui
