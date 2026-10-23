#include "m5_atoms3_voice_ui.h"

#include "esphome/core/log.h"

extern "C" {
// Include only what is used: gfx.h also pulls the touch headers, which conflict
// with the ESP-IDF 5.5 esp_lcd_io_i2c.h overloads in C++ translation units.
#include "core/gfx_core.h"
#include "core/gfx_disp.h"
#include "core/gfx_obj.h"
#include "widget/gfx_anim.h"
}

namespace esphome::m5_atoms3_voice_ui {

static const char *const TAG = "m5_atoms3_voice_ui";
// Rendering is the most deferrable work in the system, so it runs at the lowest
// usable priority: audio (19), Wi-Fi/lwIP (18), sendspin FLAC decoding (6) and
// micro wake word (3) all take precedence. The ESPHome main loop also runs at
// priority 1; UI work is isolated from it and both tasks yield regularly.
// Do not raise this above the main loop, and do not pin the task to a core:
// sendspin decodes audio on whichever core is free, and pinning the render task
// next to it starves the idle task and trips the task watchdog.
static constexpr int GFX_TASK_PRIORITY = 1;
static constexpr int GFX_TASK_CORE = -1;
static constexpr int GFX_TASK_STACK_SIZE = 8 * 1024;
static constexpr int PHASE_TASK_PRIORITY = 1;
static constexpr int PHASE_TASK_STACK_SIZE = 4 * 1024;
static constexpr uint32_t PHASE_POLL_INTERVAL_MS = 20;
// Small flush granularity keeps the internal DMA buffer cheap (h_res * rows *
// 2).
static constexpr size_t GFX_BUFFER_ROWS = 8;

void M5Atoms3VoiceUI::set_animation_source(size_t index, const uint32_t *data, size_t size) {
  this->animation_data_[index] = reinterpret_cast<const uint8_t *>(data);
  this->animation_size_[index] = size;
}

void M5Atoms3VoiceUI::setup() {
  if (this->display_ == nullptr || this->display_->is_failed()) {
    this->mark_failed(LOG_STR("Display is unavailable"));
    return;
  }

  // Rendering runs in esp_emote_gfx and EAF source changes run in the phase
  // task below, so the ESPHome main loop is not blocked by UI work.
  gfx_core_config_t core_config{};
  core_config.fps = this->fps_;
  core_config.task.task_priority = GFX_TASK_PRIORITY;
  core_config.task.task_stack = GFX_TASK_STACK_SIZE;
  core_config.task.task_affinity = GFX_TASK_CORE;
  // Library default (MALLOC_CAP_DEFAULT). No PSRAM heap override is installed by
  // this component, so this resolves to internal RAM, which keeps the render
  // task schedulable while the flash cache is disabled during NVS writes.
  core_config.task.task_stack_caps = MALLOC_CAP_DEFAULT;

  this->gfx_ = gfx_emote_init(&core_config);
  if (this->gfx_ == nullptr) {
    this->mark_failed(LOG_STR("Could not initialize esp_emote_gfx"));
    return;
  }

  gfx_disp_config_t disp_config{};
  disp_config.h_res = this->display_->get_width();
  disp_config.v_res = this->display_->get_height();
  disp_config.flush_cb = &M5Atoms3VoiceUI::flush_cb_;
  disp_config.user_data = this;
  disp_config.flags.swap = this->big_endian_;
  disp_config.flags.buff_dma = true;
  disp_config.flags.buff_spiram = false;
  disp_config.flags.double_buffer = false;
  disp_config.flags.full_frame = false;
  disp_config.buffers.buf_pixels = static_cast<size_t>(disp_config.h_res) * GFX_BUFFER_ROWS;

  this->disp_ = gfx_disp_add(this->gfx_, &disp_config);
  if (this->disp_ == nullptr) {
    this->mark_failed(LOG_STR("Could not add the display to esp_emote_gfx"));
    return;
  }
  gfx_disp_set_bg_color(this->disp_, GFX_COLOR_HEX(this->background_color_));

  // One animation object is reused for every phase: gfx_anim_set_src() frees
  // the previous decoder and frame buffer, so only one decoded frame is held in
  // RAM at a time.
  this->anim_ = gfx_anim_create(this->disp_);
  if (this->anim_ == nullptr) {
    this->mark_failed(LOG_STR("Could not create the EAF animation"));
    return;
  }
  gfx_obj_align(this->anim_, GFX_ALIGN_CENTER, 0, 0);

  // Source changes can synchronously parse the EAF header, allocate the decode
  // workspace and prepare the first frame. Keep that work out of the ESPHome
  // main loop so microphone/API servicing remains responsive.
  this->phase_task_running_ = true;
  this->phase_task_done_ = false;
  if (xTaskCreate(&M5Atoms3VoiceUI::phase_task_, "gfx_phase", PHASE_TASK_STACK_SIZE, this, PHASE_TASK_PRIORITY,
                  &this->phase_task_handle_) != pdPASS) {
    this->phase_task_running_ = false;
    this->phase_task_done_ = true;
    this->mark_failed(LOG_STR("Could not create the graphics phase task"));
  }
}

void M5Atoms3VoiceUI::loop() {}

void M5Atoms3VoiceUI::phase_task_(void *arg) {
  auto *self = static_cast<M5Atoms3VoiceUI *>(arg);
  while (self->phase_task_running_) {
    self->update_phase_();
    vTaskDelay(pdMS_TO_TICKS(PHASE_POLL_INTERVAL_MS));
  }
  self->phase_task_done_ = true;
  vTaskDelete(nullptr);
}

void M5Atoms3VoiceUI::update_phase_() {
  const int phase = *this->phase_source_;
  if (phase == this->last_phase_)
    return;
  this->last_phase_ = phase;
  this->render_phase_(phase);
}

bool M5Atoms3VoiceUI::render_phase_(int phase) {
  size_t index = 4;  // Unrecognized phases use the loading animation.
  for (size_t i = 0; i < this->phase_mapping_.size(); i++) {
    if (phase == this->phase_mapping_[i]) {
      index = i;
      break;
    }
  }
  // Optional muted/timer animations fall back to idle/replying.
  if (this->animation_data_[index] == nullptr)
    index = index == 6 ? 0 : 3;

  if (static_cast<int>(index) == this->active_animation_)
    return true;
  if (this->gfx_ == nullptr || this->anim_ == nullptr)
    return false;

  if (gfx_emote_lock(this->gfx_) != ESP_OK) {
    ESP_LOGE(TAG, "Could not lock the graphics context");
    return false;
  }
  esp_err_t err = gfx_anim_set_src(this->anim_, this->animation_data_[index], this->animation_size_[index]);
  if (err == ESP_OK)
    err = gfx_anim_set_segment(this->anim_, 0, UINT32_MAX, this->fps_, true);
  if (err == ESP_OK)
    err = gfx_anim_start(this->anim_);
  gfx_obj_align(this->anim_, GFX_ALIGN_CENTER, 0, 0);
  gfx_emote_unlock(this->gfx_);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Could not start the animation for phase %d: %s", phase, esp_err_to_name(err));
    this->active_animation_ = -1;
    return false;
  }
  ESP_LOGD(TAG, "Rendering phase %d (animation %zu)", phase, index);
  this->active_animation_ = static_cast<int>(index);
  return true;
}

void M5Atoms3VoiceUI::flush_cb_(gfx_disp_t *disp, int x1, int y1, int x2, int y2, const void *data) {
  auto *self = static_cast<M5Atoms3VoiceUI *>(gfx_disp_get_user_data(disp));
  if (self != nullptr && data != nullptr)
    self->flush_(x1, y1, x2, y2, data);
  // The ESPHome display write is synchronous and there is no second buffer, so
  // the same buffer is reused and no swap is requested.
  gfx_disp_flush_ready(disp, false);
}

void M5Atoms3VoiceUI::flush_(int x1, int y1, int x2, int y2, const void *data) {
  const int width = x2 - x1;
  const int height = y2 - y1;
  if (width <= 0 || height <= 0)
    return;
  this->display_->draw_pixels_at(x1, y1, width, height, static_cast<const uint8_t *>(data), display::COLOR_ORDER_RGB,
                                 display::COLOR_BITNESS_565, this->big_endian_);
}

void M5Atoms3VoiceUI::on_shutdown() {
  if (this->phase_task_handle_ != nullptr) {
    this->phase_task_running_ = false;
    while (!this->phase_task_done_) {
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    this->phase_task_handle_ = nullptr;
  }
  if (this->gfx_ != nullptr) {
    // Releases the displays, their children and the render task.
    gfx_emote_deinit(this->gfx_);
    this->gfx_ = nullptr;
    this->disp_ = nullptr;
    this->anim_ = nullptr;
  }
}

void M5Atoms3VoiceUI::dump_config() {
  ESP_LOGCONFIG(TAG, "M5 AtomS3 Voice UI:");
  if (this->display_ != nullptr)
    ESP_LOGCONFIG(TAG, "  Display: %d x %d", this->display_->get_width(), this->display_->get_height());
  ESP_LOGCONFIG(TAG, "  Frame rate: %u fps", static_cast<unsigned>(this->fps_));
  ESP_LOGCONFIG(TAG, "  Big endian: %s", YESNO(this->big_endian_));
  ESP_LOGCONFIG(TAG, "  Voice assistant configured: %s", YESNO(this->voice_assistant_ != nullptr));
}

}  // namespace esphome::m5_atoms3_voice_ui
