#include "m5_sticks3_ui.h"

namespace esphome::m5_sticks3_ui {

ESPHOME_LOG_TAG(TAG, "m5_sticks3_ui");

void M5Sticks3UI::setup() {
#ifdef USE_LVGL
  auto *screen = lv_screen_active();
  if (screen == nullptr) {
    this->mark_failed();
    return;
  }

  auto *label = lv_label_create(screen);
  lv_label_set_text(label, "Hello");
  lv_obj_center(label);
#else
  this->mark_failed();
#endif
}

void M5Sticks3UI::dump_config() {
  ESP_LOGCONFIG(TAG, "StickS3 LVGL UI:");
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}

}  // namespace esphome::m5_sticks3_ui
