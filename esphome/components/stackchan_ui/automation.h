#pragma once

#include <string>

#include "esphome/core/automation.h"

#include "stackchan_ui.h"

namespace esphome::stackchan_ui {

template<typename... Ts> class SetEmotionAction final : public Action<Ts...>, public Parented<StackChanUI> {
 public:
  TEMPLATABLE_VALUE(std::string, emotion)

  void play(const Ts &...x) override { this->parent_->set_emotion(this->emotion_.value(x...)); }
};

template<typename... Ts> class SetSpeechAction final : public Action<Ts...>, public Parented<StackChanUI> {
 public:
  TEMPLATABLE_VALUE(std::string, text)

  void set_duration(uint32_t duration_ms) { this->duration_ms_ = duration_ms; }
  void set_speak(bool speak) { this->speak_ = speak; }

  void play(const Ts &...x) override {
    const std::string text = this->text_.value(x...);
    if (this->speak_) {
      this->parent_->say(text, this->duration_ms_);
    } else {
      this->parent_->set_speech(text, this->duration_ms_);
    }
  }

 protected:
  uint32_t duration_ms_{0};
  bool speak_{false};
};

}  // namespace esphome::stackchan_ui
