#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "esphome/components/font/font.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "assets.h"

namespace esphome::stackchan_ui {

/// Emotions exposed by the original StackChan avatar.
enum class Emotion : uint8_t {
  EMOTION_NEUTRAL = 0,
  EMOTION_HAPPY,
  EMOTION_ANGRY,
  EMOTION_SAD,
  EMOTION_DOUBT,
  EMOTION_SLEEPY,
};

/// An LVGL reproduction of the StackChan face.
///
/// The face is built from plain LVGL objects, matching the original firmware:
/// a 320x240 panel, two eyes (white ball plus a black eyelid mask), a rounded
/// mouth and a speech bubble. The behaviours are time based and run in
/// ``loop()`` on the ESPHome main loop, which is also the thread that services
/// LVGL.
class StackChanUI final : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::PROCESSOR - 10.0f; }

  void set_font(const font::Font *font) { this->font_ = font; }
  void set_primary_color(uint32_t color) { this->primary_color_ = color; }
  void set_secondary_color(uint32_t color) { this->secondary_color_ = color; }
  void set_panel_size(int width, int height) {
    this->panel_width_ = width;
    this->panel_height_ = height;
  }

  void set_blink_open_interval(uint32_t ms) { this->blink_open_interval_ms_ = ms; }
  void set_blink_close_interval(uint32_t ms) { this->blink_close_interval_ms_ = ms; }
  void set_breath_cycle(uint32_t ms) { this->breath_cycle_ms_ = ms; }
  void set_breath_update_interval(uint32_t ms) { this->breath_update_interval_ms_ = ms; }
  void set_breath_amplitude(int amplitude) { this->breath_amplitude_ = amplitude; }
  void set_random_interval(uint32_t min_ms, uint32_t max_ms) {
    this->random_interval_min_ms_ = min_ms;
    this->random_interval_max_ms_ = max_ms;
  }
  void set_head_pet_timing(uint32_t min_ms, uint32_t max_ms, uint32_t restore_ms) {
    this->head_pet_min_ms_ = min_ms;
    this->head_pet_max_ms_ = max_ms;
    this->head_pet_restore_ms_ = restore_ms;
  }
  void set_shake_duration(uint32_t ms) { this->shake_duration_ms_ = ms; }
  void set_speaking_interval(uint32_t ms) { this->speaking_interval_ms_ = ms; }
  void set_enable_blink(bool enabled) { this->enable_blink_ = enabled; }
  void set_enable_breath(bool enabled) { this->enable_breath_ = enabled; }
  void set_enable_random(bool enabled) { this->enable_random_ = enabled; }

  /// Set an emotion. Accepts the original firmware names plus the aliases
  /// ``laughing``, ``crying`` and ``doubtful``.
  void set_emotion(const std::string &emotion);
  /// Show text in the speech bubble. A non-zero duration hides it automatically.
  void set_speech(const std::string &text, uint32_t duration_ms);
  /// Show text and animate the mouth until the bubble duration expires.
  void say(const std::string &text, uint32_t duration_ms);
  void clear_speech();
  /// React as if the head was petted: happy eyes, heart and blush.
  void head_pet();
  /// Start a pet reaction without arming the restore timer. Used while a touch
  /// gesture is still in progress; call ``head_pet_release`` when it ends.
  void head_pet_hold();
  /// Arm the restore timer after a held pet gesture.
  void head_pet_release();
  /// React as if the robot was shaken: hidden eyes, dizzy and blush.
  void shake();
  /// Open and close the mouth while the voice assistant speaks.
  void start_speaking();
  void stop_speaking();

  template<typename F> void add_on_head_pet_callback(F &&callback) {
    this->head_pet_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_head_pet_restore_callback(F &&callback) {
    this->head_pet_restore_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_shake_callback(F &&callback) {
    this->shake_callback_.add(std::forward<F>(callback));
  }

 protected:
  struct Eye {
    lv_obj_t *container{nullptr};
    lv_obj_t *ball{nullptr};
    lv_obj_t *eyelid{nullptr};
    bool is_left{false};
    bool visible{true};
    int weight{100};
    int size{0};
    int rotation{0};
    int position_x{0};
    int position_y{0};
  };

  struct Mouth {
    lv_obj_t *obj{nullptr};
    bool visible{true};
    int weight{0};
    int rotation{0};
    int position_x{0};
    int position_y{0};
  };

  void create_face_();
  void create_decorators_();
  void apply_emotion_(Emotion emotion);
  void refresh_eyes_();
  void refresh_mouth_();
  void refresh_face_();
  void start_head_pet_(bool hold);

  void update_blink_(uint32_t now);
  void update_breath_(uint32_t now);
  void update_idle_expression_(uint32_t now);
  void update_speaking_(uint32_t now);
  void update_decorators_(uint32_t now);
  void update_head_pet_(uint32_t now);
  void update_shake_(uint32_t now);
  void update_speech_(uint32_t now);

  static Emotion parse_emotion_(const std::string &emotion);
  static int map_range_(int value, int in_min, int in_max, int out_min, int out_max);
  static int random_between_(int min, int max);
  static void show_(lv_obj_t *obj);
  static void hide_(lv_obj_t *obj);

  lv_obj_t *panel_{nullptr};
  Eye left_eye_{};
  Eye right_eye_{};
  Mouth mouth_{};

  lv_obj_t *bubble_container_{nullptr};
  lv_obj_t *bubble_{nullptr};
  lv_obj_t *bubble_arrow_{nullptr};
  lv_obj_t *bubble_text_{nullptr};

  lv_obj_t *heart_{nullptr};
  lv_obj_t *dizzy_left_{nullptr};
  lv_obj_t *dizzy_right_{nullptr};
  lv_obj_t *shy_left_{nullptr};
  lv_obj_t *shy_right_{nullptr};

  const font::Font *font_{nullptr};
  uint32_t primary_color_{0xFFFFFF};
  uint32_t secondary_color_{0x000000};
  int panel_width_{320};
  int panel_height_{240};

  bool enable_blink_{true};
  bool enable_breath_{true};
  bool enable_random_{true};

  uint32_t blink_open_interval_ms_{5200};
  uint32_t blink_close_interval_ms_{200};
  bool blink_closed_{false};
  uint32_t blink_next_tick_{0};

  uint32_t breath_cycle_ms_{6600};
  uint32_t breath_update_interval_ms_{600};
  int breath_amplitude_{16};
  uint32_t breath_start_tick_{0};
  uint32_t breath_last_tick_{0};
  int breath_offset_{0};

  uint32_t random_interval_min_ms_{2000};
  uint32_t random_interval_max_ms_{6000};
  uint32_t random_next_tick_{0};

  uint32_t head_pet_min_ms_{1500};
  uint32_t head_pet_max_ms_{2500};
  uint32_t head_pet_restore_ms_{3000};
  bool head_pet_active_{false};
  bool head_pet_holding_{false};
  uint32_t head_pet_until_{0};
  Emotion head_pet_previous_emotion_{Emotion::EMOTION_NEUTRAL};

  bool shake_active_{false};
  uint32_t shake_until_{0};
  uint32_t shake_duration_ms_{4000};
  bool modify_locked_{false};
  bool mouth_wiggle_phase_{false};
  uint32_t mouth_wiggle_next_tick_{0};

  bool speaking_{false};
  bool mouth_open_{false};
  uint32_t speaking_interval_ms_{180};
  uint32_t mouth_next_tick_{0};

  uint32_t heart_until_{0};
  uint32_t heart_next_tick_{0};
  int heart_frame_{0};
  uint32_t shy_until_{0};
  bool shy_shake_active_{false};
  bool dizzy_active_{false};
  uint32_t dizzy_next_tick_{0};
  int dizzy_rotation_{0};

  uint32_t speech_until_{0};
  bool speech_auto_stop_{false};
  Emotion emotion_{Emotion::EMOTION_NEUTRAL};

  LazyCallbackManager<void()> head_pet_callback_{};
  LazyCallbackManager<void()> head_pet_restore_callback_{};
  LazyCallbackManager<void()> shake_callback_{};
};

}  // namespace esphome::stackchan_ui
