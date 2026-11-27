#include "stackchan_ui.h"

#include <algorithm>
#include <cctype>
#include <cinttypes>
#include <cmath>

#include "esphome/core/helpers.h"

namespace esphome::stackchan_ui {

ESPHOME_LOG_TAG(TAG, "stackchan_ui");

namespace {
constexpr float PI = 3.14159265358979323846f;
}  // namespace

void StackChanUI::setup() {
  if (this->font_ == nullptr) {
    ESP_LOGE(TAG, "A font is required for the speech bubble");
    this->mark_failed();
    return;
  }
  if (lv_screen_active() == nullptr) {
    ESP_LOGE(TAG, "No active LVGL screen");
    this->mark_failed();
    return;
  }

  this->create_face_();
  this->create_decorators_();

  const uint32_t now = millis();
  this->breath_start_tick_ = now;
  this->breath_last_tick_ = now;
  this->blink_next_tick_ = now + this->blink_open_interval_ms_;
  this->random_next_tick_ = now + 500;
  this->mouth_next_tick_ = now;
  this->mouth_wiggle_next_tick_ = now;

  this->emotion_ = Emotion::EMOTION_NEUTRAL;
  this->apply_emotion_(this->emotion_);
  this->clear_speech();
  this->refresh_face_();
}

void StackChanUI::create_face_() {
  this->panel_ = lv_obj_create(lv_screen_active());
  lv_obj_set_size(this->panel_, this->panel_width_, this->panel_height_);
  lv_obj_align(this->panel_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_radius(this->panel_, 0, 0);
  lv_obj_set_style_border_width(this->panel_, 0, 0);
  lv_obj_set_style_bg_color(this->panel_, lv_color_hex(this->secondary_color_), 0);
  lv_obj_set_style_bg_opa(this->panel_, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(this->panel_, 0, 0);
  lv_obj_set_scrollable(this->panel_, false);

  auto create_eye = [this](Eye &eye, bool is_left) {
    eye.is_left = is_left;
    eye.container = lv_obj_create(this->panel_);
    lv_obj_set_size(eye.container, 32, 32);
    lv_obj_set_style_radius(eye.container, 0, 0);
    lv_obj_set_style_border_width(eye.container, 0, 0);
    lv_obj_set_style_bg_opa(eye.container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(eye.container, 0, 0);
    lv_obj_set_style_transform_pivot_x(eye.container, 16, 0);
    lv_obj_set_style_transform_pivot_y(eye.container, 16, 0);
    lv_obj_set_scrollable(eye.container, false);

    eye.ball = lv_obj_create(eye.container);
    lv_obj_align(eye.ball, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(eye.ball, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(eye.ball, 0, 0);
    lv_obj_set_style_bg_color(eye.ball, lv_color_hex(this->primary_color_), 0);
    lv_obj_set_style_bg_opa(eye.ball, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(eye.ball, false);

    eye.eyelid = lv_obj_create(eye.container);
    lv_obj_align(eye.eyelid, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(eye.eyelid, 0, 0);
    lv_obj_set_style_border_width(eye.eyelid, 0, 0);
    lv_obj_set_style_bg_color(eye.eyelid, lv_color_hex(this->secondary_color_), 0);
    lv_obj_set_style_bg_opa(eye.eyelid, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(eye.eyelid, false);

    // Eyelid offsets are applied in refresh_eyes_() after the initial size is known.
  };

  create_eye(this->left_eye_, true);
  create_eye(this->right_eye_, false);

  this->mouth_.obj = lv_obj_create(this->panel_);
  lv_obj_align(this->mouth_.obj, LV_ALIGN_CENTER, 0, 26);
  lv_obj_set_style_border_width(this->mouth_.obj, 0, 0);
  lv_obj_set_style_bg_color(this->mouth_.obj, lv_color_hex(this->primary_color_), 0);
  lv_obj_set_style_bg_opa(this->mouth_.obj, LV_OPA_COVER, 0);
  lv_obj_set_scrollable(this->mouth_.obj, false);

  this->bubble_container_ = lv_obj_create(this->panel_);
  lv_obj_set_size(this->bubble_container_, this->panel_width_, 74);
  lv_obj_align(this->bubble_container_, LV_ALIGN_CENTER, 0, 89);
  lv_obj_set_style_radius(this->bubble_container_, 0, 0);
  lv_obj_set_style_border_width(this->bubble_container_, 0, 0);
  lv_obj_set_style_bg_opa(this->bubble_container_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(this->bubble_container_, 0, 0);
  lv_obj_set_scrollable(this->bubble_container_, false);

  this->bubble_arrow_ = lv_image_create(this->bubble_container_);
  lv_image_set_src(this->bubble_arrow_, &stackchan_ui_bubble_arrow);
  lv_obj_align(this->bubble_arrow_, LV_ALIGN_CENTER, 40, -15);
  lv_obj_set_style_image_recolor_opa(this->bubble_arrow_, LV_OPA_COVER, 0);
  lv_obj_set_style_image_recolor(this->bubble_arrow_, lv_color_hex(this->primary_color_), 0);

  this->bubble_ = lv_obj_create(this->bubble_container_);
  lv_obj_set_size(this->bubble_, 340, 52);
  lv_obj_align(this->bubble_, LV_ALIGN_CENTER, 0, 11);
  lv_obj_set_style_radius(this->bubble_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(this->bubble_, 0, 0);
  lv_obj_set_style_bg_color(this->bubble_, lv_color_hex(this->primary_color_), 0);
  lv_obj_set_style_bg_opa(this->bubble_, LV_OPA_COVER, 0);
  lv_obj_set_scrollable(this->bubble_, false);

  this->bubble_text_ = lv_label_create(this->bubble_);
  lv_obj_set_width(this->bubble_text_, 280);
  lv_obj_align(this->bubble_text_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_text_color(this->bubble_text_, lv_color_hex(this->secondary_color_), 0);
  lv_obj_set_style_text_align(this->bubble_text_, LV_TEXT_ALIGN_CENTER, 0);
  lvgl::lv_obj_set_style_text_font(this->bubble_text_, this->font_, LV_PART_MAIN);
  lv_label_set_long_mode(this->bubble_text_, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
  lv_label_set_text(this->bubble_text_, "");
}

void StackChanUI::create_decorators_() {
  auto *parent = lv_layer_top();
  if (parent == nullptr)
    parent = lv_screen_active();

  auto create_image = [parent](const lv_image_dsc_t *src, int x, int y, uint32_t color, int pivot_x, int pivot_y) {
    auto *img = lv_image_create(parent);
    lv_image_set_src(img, src);
    lv_obj_align(img, LV_ALIGN_CENTER, x, y);
    lv_obj_set_style_transform_pivot_x(img, pivot_x, 0);
    lv_obj_set_style_transform_pivot_y(img, pivot_y, 0);
    lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, 0);
    lv_obj_set_style_image_recolor(img, lv_color_hex(color), 0);
    lv_obj_set_hidden(img, true);
    return img;
  };

  this->heart_ = create_image(&stackchan_ui_decorator_heart, 108, -70, 0xE13232, 20, 20);
  this->dizzy_left_ = create_image(&stackchan_ui_decorator_dizzy, -70, -16, 0xFFFFFF, 16, 18);
  this->dizzy_right_ = create_image(&stackchan_ui_decorator_dizzy, 70, -16, 0xFFFFFF, 16, 18);
  this->shy_left_ = create_image(&stackchan_ui_decorator_shy, -108, 28, 0xF7A59E, 19, 9);
  this->shy_right_ = create_image(&stackchan_ui_decorator_shy, 108, 28, 0xF7A59E, 19, 9);
}

void StackChanUI::set_emotion(const std::string &emotion) {
  if (this->left_eye_.container == nullptr)
    return;

  const Emotion parsed = parse_emotion_(emotion);
  if (parsed == Emotion::EMOTION_NEUTRAL && emotion != "neutral") {
    ESP_LOGW(TAG, "Unknown emotion '%s', using neutral", emotion.c_str());
  }

  // An explicit emotion wins over the temporary head-pet state.
  if (this->head_pet_active_) {
    this->head_pet_active_ = false;
    this->head_pet_holding_ = false;
    this->head_pet_until_ = 0;
    this->heart_until_ = 0;
    if (!this->shake_active_)
      this->shy_until_ = 0;
    this->head_pet_restore_callback_.call();
  }
  this->emotion_ = parsed;
  this->apply_emotion_(parsed);
}

void StackChanUI::apply_emotion_(Emotion emotion) {
  int weight = 100;
  int rotation = 0;
  switch (emotion) {
    case Emotion::EMOTION_HAPPY:
      weight = 72;
      rotation = 1550;
      break;
    case Emotion::EMOTION_ANGRY:
      weight = 70;
      rotation = 450;
      break;
    case Emotion::EMOTION_SAD:
      weight = 70;
      rotation = -400;
      break;
    case Emotion::EMOTION_DOUBT:
      weight = 75;
      rotation = 0;
      break;
    case Emotion::EMOTION_SLEEPY:
      weight = 35;
      rotation = -50;
      break;
    case Emotion::EMOTION_NEUTRAL:
    default:
      weight = 100;
      rotation = 0;
      break;
  }

  // Native code stores 0.1 degree units. Negative angles are wrapped rather
  // than clamped so the right eye mirrors the left as intended.
  const int left_rotation = (rotation + 3600) % 3600;
  const int right_rotation = (3600 - left_rotation) % 3600;
  this->left_eye_.weight = weight;
  this->left_eye_.rotation = left_rotation;
  this->right_eye_.weight = weight;
  this->right_eye_.rotation = right_rotation;
  this->refresh_eyes_();
}

void StackChanUI::refresh_eyes_() {
  auto refresh_eye = [this](Eye &eye) {
    const int x = (eye.is_left ? -70 : 70) + map_range_(eye.position_x, -100, 100, -16, 16);
    const int y_normalized = std::clamp(eye.position_y + this->breath_offset_, -100, 100);
    const int y = -16 + map_range_(y_normalized, -100, 100, -16, 16);
    lv_obj_align(eye.container, LV_ALIGN_CENTER, x, y);

    const int eye_size = map_range_(eye.size, -100, 100, 8, 32);
    lv_obj_set_size(eye.ball, eye_size, eye_size);
    lv_obj_set_size(eye.eyelid, eye_size, eye_size);

    const int effective_weight = this->blink_closed_ ? 25 : eye.weight;
    lv_obj_set_y(eye.eyelid, -(effective_weight * eye_size) / 100);
    lv_obj_set_style_transform_rotation(eye.container, eye.rotation, 0);

    if (eye.visible)
      show_(eye.container);
    else
      hide_(eye.container);
  };

  refresh_eye(this->left_eye_);
  refresh_eye(this->right_eye_);
}

void StackChanUI::refresh_mouth_() {
  const int width = map_range_(this->mouth_.weight, 0, 100, 90, 60);
  const int height = map_range_(this->mouth_.weight, 0, 100, 6, 50);
  const int radius = map_range_(this->mouth_.weight, 0, 100, 0, 16);
  lv_obj_set_size(this->mouth_.obj, width, height);
  lv_obj_set_style_radius(this->mouth_.obj, radius, 0);

  const int x = map_range_(this->mouth_.position_x, -100, 100, -16, 16);
  const int y_normalized = std::clamp(this->mouth_.position_y + this->breath_offset_, -100, 100);
  const int y = 26 + map_range_(y_normalized, -100, 100, -16, 16);
  lv_obj_align(this->mouth_.obj, LV_ALIGN_CENTER, x, y);

  lv_obj_set_style_transform_pivot_x(this->mouth_.obj, width / 2, 0);
  lv_obj_set_style_transform_pivot_y(this->mouth_.obj, height / 2, 0);
  lv_obj_set_style_transform_rotation(this->mouth_.obj, this->mouth_.rotation, 0);

  if (this->mouth_.visible)
    show_(this->mouth_.obj);
  else
    hide_(this->mouth_.obj);
}

void StackChanUI::refresh_face_() {
  this->refresh_eyes_();
  this->refresh_mouth_();
}

void StackChanUI::set_speech(const std::string &text, uint32_t duration_ms) {
  if (this->bubble_text_ == nullptr)
    return;
  if (text.empty()) {
    this->clear_speech();
    return;
  }

  lv_label_set_text(this->bubble_text_, text.c_str());
  lv_point_t text_size{};
  lv_text_get_size(&text_size, text.c_str(), lv_obj_get_style_text_font(this->bubble_text_, LV_PART_MAIN), 0, 0,
                   LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  const int bubble_width = std::clamp(static_cast<int>(text_size.x) + 40, 90, 340);
  const int bubble_offset = map_range_(bubble_width, 90, 340, 66, 0);
  lv_obj_set_width(this->bubble_, bubble_width);
  lv_obj_set_x(this->bubble_, bubble_offset);
  show_(this->bubble_container_);

  this->speech_until_ = duration_ms == 0 ? 0 : millis() + duration_ms;
}

void StackChanUI::say(const std::string &text, uint32_t duration_ms) {
  if (this->mouth_.obj == nullptr || this->bubble_text_ == nullptr)
    return;
  if (text.empty()) {
    this->clear_speech();
    this->stop_speaking();
    return;
  }

  this->set_speech(text, duration_ms);
  this->start_speaking();
  this->speech_auto_stop_ = duration_ms != 0;
}

void StackChanUI::clear_speech() {
  if (this->bubble_text_ == nullptr) {
    this->speech_until_ = 0;
    return;
  }
  lv_label_set_text(this->bubble_text_, "");
  hide_(this->bubble_container_);
  this->speech_until_ = 0;
  if (this->speech_auto_stop_) {
    this->speech_auto_stop_ = false;
    this->stop_speaking();
  }
}

void StackChanUI::head_pet() { this->start_head_pet_(false); }

void StackChanUI::head_pet_hold() { this->start_head_pet_(true); }

void StackChanUI::head_pet_release() {
  if (!this->head_pet_active_)
    return;
  this->head_pet_holding_ = false;
  this->head_pet_until_ = millis() + this->head_pet_restore_ms_;
}

void StackChanUI::start_head_pet_(bool hold) {
  if (this->heart_ == nullptr)
    return;

  const uint32_t now = millis();
  if (!this->head_pet_active_) {
    this->head_pet_previous_emotion_ = this->emotion_;
    this->head_pet_active_ = true;
  }
  this->emotion_ = Emotion::EMOTION_HAPPY;
  this->apply_emotion_(this->emotion_);
  this->head_pet_holding_ = hold;
  this->head_pet_until_ = hold ? 0 : now + this->head_pet_restore_ms_;

  const uint32_t duration = random_between_(this->head_pet_min_ms_, this->head_pet_max_ms_);
  this->heart_until_ = now + duration;
  this->shy_until_ = std::max(this->shy_until_, now + duration);
  this->heart_frame_ = 0;
  lv_obj_set_style_transform_rotation(this->heart_, 150, 0);
  this->heart_next_tick_ = now + 500;
  this->head_pet_callback_.call();
}

void StackChanUI::shake() {
  if (this->dizzy_left_ == nullptr)
    return;

  const uint32_t now = millis();
  if (!this->shake_active_) {
    this->shake_active_ = true;
    this->dizzy_active_ = true;
    this->shy_shake_active_ = true;
    this->modify_locked_ = true;
    this->dizzy_rotation_ = 0;
    lv_obj_set_style_transform_rotation(this->dizzy_left_, 0, 0);
    lv_obj_set_style_transform_rotation(this->dizzy_right_, 450, 0);
    this->dizzy_next_tick_ = now + 300;
    this->mouth_wiggle_phase_ = false;
    this->mouth_wiggle_next_tick_ = now;
    this->left_eye_.visible = false;
    this->right_eye_.visible = false;
    this->refresh_eyes_();
  }
  this->shake_until_ = now + this->shake_duration_ms_;
  this->shake_callback_.call();
}

void StackChanUI::start_speaking() {
  if (this->mouth_.obj == nullptr)
    return;
  this->speech_auto_stop_ = false;
  this->speaking_ = true;
  this->mouth_open_ = false;
  this->mouth_next_tick_ = millis();
}

void StackChanUI::stop_speaking() {
  this->speech_auto_stop_ = false;
  this->speaking_ = false;
  if (this->mouth_.obj == nullptr)
    return;
  this->mouth_.weight = 0;
  this->refresh_mouth_();
}

void StackChanUI::update_blink_(uint32_t now) {
  if (!this->enable_blink_ || this->modify_locked_)
    return;
  if (now < this->blink_next_tick_)
    return;

  if (!this->blink_closed_) {
    this->blink_closed_ = true;
    this->blink_next_tick_ = now + this->blink_close_interval_ms_;
  } else {
    this->blink_closed_ = false;
    this->blink_next_tick_ = now + this->blink_open_interval_ms_ + random_between_(0, 500);
  }
  this->refresh_eyes_();
}

void StackChanUI::update_breath_(uint32_t now) {
  if (!this->enable_breath_) {
    if (this->breath_offset_ != 0) {
      this->breath_offset_ = 0;
      this->refresh_face_();
    }
    return;
  }
  if (now - this->breath_last_tick_ < this->breath_update_interval_ms_)
    return;
  this->breath_last_tick_ = now;

  const uint32_t cycle = std::max<uint32_t>(this->breath_cycle_ms_, 1);
  const float phase = static_cast<float>((now - this->breath_start_tick_) % cycle) / static_cast<float>(cycle);
  const float wave = sinf(phase * 2.0f * PI);
  this->breath_offset_ = static_cast<int>(wave * static_cast<float>(this->breath_amplitude_));
  this->refresh_face_();
}

void StackChanUI::update_idle_expression_(uint32_t now) {
  if (!this->enable_random_ || this->modify_locked_)
    return;
  if (now < this->random_next_tick_)
    return;

  const int action = random_between_(0, 99);
  if (action < 70) {
    const int x = random_between_(-20, 20);
    const int y = random_between_(-15, 15);
    this->left_eye_.position_x = x;
    this->left_eye_.position_y = y;
    this->right_eye_.position_x = x;
    this->right_eye_.position_y = y;
    this->mouth_.position_y = random_between_(0, 10);
  } else if (action < 80) {
    const int rotation = random_between_(-30, 30);
    this->mouth_.rotation = rotation < 0 ? 3600 + rotation : rotation;
  } else {
    this->left_eye_.position_x = 0;
    this->left_eye_.position_y = 0;
    this->right_eye_.position_x = 0;
    this->right_eye_.position_y = 0;
    this->mouth_.position_x = 0;
    this->mouth_.position_y = 0;
    this->left_eye_.size = 0;
    this->right_eye_.size = 0;
    this->mouth_.rotation = 0;
    this->mouth_.weight = 0;
  }

  this->refresh_face_();
  this->random_next_tick_ = now + random_between_(this->random_interval_min_ms_, this->random_interval_max_ms_);
}

void StackChanUI::update_speaking_(uint32_t now) {
  if (!this->speaking_ || this->shake_active_)
    return;
  if (now < this->mouth_next_tick_)
    return;

  this->mouth_open_ = !this->mouth_open_;
  this->mouth_.weight = this->mouth_open_ ? random_between_(40, 80) : random_between_(0, 20);
  this->mouth_next_tick_ = now + this->speaking_interval_ms_;
  this->refresh_mouth_();
}

void StackChanUI::update_head_pet_(uint32_t now) {
  if (!this->head_pet_active_ || this->head_pet_holding_ || now < this->head_pet_until_)
    return;

  this->head_pet_active_ = false;
  this->head_pet_holding_ = false;
  this->emotion_ = this->head_pet_previous_emotion_;
  this->apply_emotion_(this->emotion_);
  this->head_pet_restore_callback_.call();
}

void StackChanUI::update_shake_(uint32_t now) {
  if (!this->shake_active_)
    return;

  if (now >= this->shake_until_) {
    this->shake_active_ = false;
    this->dizzy_active_ = false;
    this->shy_shake_active_ = false;
    this->modify_locked_ = false;
    this->blink_closed_ = false;
    this->left_eye_.visible = true;
    this->right_eye_.visible = true;
    this->mouth_.rotation = 0;
    this->mouth_.weight = 0;
    this->refresh_face_();
    return;
  }

  if (now >= this->mouth_wiggle_next_tick_) {
    this->mouth_wiggle_phase_ = !this->mouth_wiggle_phase_;
    this->mouth_.rotation = this->mouth_wiggle_phase_ ? 25 : 3575;
    this->mouth_.weight = 65;
    this->mouth_wiggle_next_tick_ = now + 600;
    this->refresh_mouth_();
  }
}

void StackChanUI::update_decorators_(uint32_t now) {
  if (this->heart_until_ != 0 && now >= this->heart_until_) {
    this->heart_until_ = 0;
  }
  if (this->heart_until_ != 0) {
    show_(this->heart_);
    if (now >= this->heart_next_tick_) {
      this->heart_frame_ = (this->heart_frame_ + 1) % 2;
      lv_obj_set_style_transform_rotation(this->heart_, this->heart_frame_ == 0 ? 150 : 200, 0);
      this->heart_next_tick_ = now + 500;
    }
  } else {
    hide_(this->heart_);
  }

  if (!this->shy_shake_active_ && this->shy_until_ != 0 && now >= this->shy_until_) {
    this->shy_until_ = 0;
  }
  if (this->shy_shake_active_ || this->shy_until_ != 0) {
    show_(this->shy_left_);
    show_(this->shy_right_);
  } else {
    hide_(this->shy_left_);
    hide_(this->shy_right_);
  }

  if (this->dizzy_active_) {
    show_(this->dizzy_left_);
    show_(this->dizzy_right_);
    if (now >= this->dizzy_next_tick_) {
      this->dizzy_rotation_ = (this->dizzy_rotation_ + 300) % 3600;
      lv_obj_set_style_transform_rotation(this->dizzy_left_, (3600 - this->dizzy_rotation_) % 3600, 0);
      lv_obj_set_style_transform_rotation(this->dizzy_right_, (3600 - this->dizzy_rotation_ + 450) % 3600, 0);
      this->dizzy_next_tick_ = now + 300;
    }
  } else {
    hide_(this->dizzy_left_);
    hide_(this->dizzy_right_);
  }
}

void StackChanUI::update_speech_(uint32_t now) {
  if (this->speech_until_ != 0 && now >= this->speech_until_)
    this->clear_speech();
}

void StackChanUI::loop() {
  const uint32_t now = millis();
  this->update_blink_(now);
  this->update_breath_(now);
  this->update_idle_expression_(now);
  this->update_speaking_(now);
  this->update_head_pet_(now);
  this->update_shake_(now);
  this->update_decorators_(now);
  this->update_speech_(now);
}

Emotion StackChanUI::parse_emotion_(const std::string &emotion) {
  std::string normalized;
  normalized.reserve(emotion.size());
  for (char c : emotion)
    normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));

  if (normalized == "happy" || normalized == "laughing")
    return Emotion::EMOTION_HAPPY;
  if (normalized == "angry")
    return Emotion::EMOTION_ANGRY;
  if (normalized == "sad" || normalized == "crying")
    return Emotion::EMOTION_SAD;
  if (normalized == "doubt" || normalized == "doubtful")
    return Emotion::EMOTION_DOUBT;
  if (normalized == "sleepy")
    return Emotion::EMOTION_SLEEPY;
  return Emotion::EMOTION_NEUTRAL;
}

int StackChanUI::map_range_(int value, int in_min, int in_max, int out_min, int out_max) {
  if (in_max == in_min)
    return out_min;
  return static_cast<int>(lroundf(static_cast<float>(out_min) +
                                  (static_cast<float>(value - in_min) * static_cast<float>(out_max - out_min)) /
                                      static_cast<float>(in_max - in_min)));
}

int StackChanUI::random_between_(int min, int max) {
  if (max <= min)
    return min;
  return min + static_cast<int>(random_uint32() % static_cast<uint32_t>(max - min + 1));
}

void StackChanUI::show_(lv_obj_t *obj) {
  if (obj != nullptr && lv_obj_is_hidden(obj))
    lv_obj_set_hidden(obj, false);
}

void StackChanUI::hide_(lv_obj_t *obj) {
  if (obj != nullptr && !lv_obj_is_hidden(obj))
    lv_obj_set_hidden(obj, true);
}

void StackChanUI::dump_config() {
  ESP_LOGCONFIG(TAG, "StackChan UI:");
  ESP_LOGCONFIG(TAG, "  Panel: %d x %d", this->panel_width_, this->panel_height_);
  ESP_LOGCONFIG(TAG, "  Blink: %s, breath: %s, random: %s", YESNO(this->enable_blink_), YESNO(this->enable_breath_),
                YESNO(this->enable_random_));
  ESP_LOGCONFIG(TAG, "  Blink interval: %" PRIu32 " / %" PRIu32 " ms", this->blink_open_interval_ms_,
                this->blink_close_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Breath cycle: %" PRIu32 " ms, amplitude: %d", this->breath_cycle_ms_, this->breath_amplitude_);
  ESP_LOGCONFIG(TAG, "  Speaking interval: %" PRIu32 " ms", this->speaking_interval_ms_);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}

}  // namespace esphome::stackchan_ui
