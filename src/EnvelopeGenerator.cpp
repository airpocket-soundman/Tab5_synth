#include "EnvelopeGenerator.h"

#include <algorithm>

void EnvelopeGenerator::setSettings(const EnvelopeSettings& settings) {
  settings_ = settings;
  settings_.attack_seconds = std::max(0.0f, settings_.attack_seconds);
  settings_.decay_seconds = std::max(0.0f, settings_.decay_seconds);
  settings_.sustain_level = std::clamp(settings_.sustain_level, 0.0f, 1.0f);
  settings_.release_seconds = std::max(0.0f, settings_.release_seconds);
}

void EnvelopeGenerator::reset() {
  stage_ = Stage::Idle;
  value_ = 0.0f;
  release_start_level_ = 0.0f;
}

void EnvelopeGenerator::noteOn() {
  if (settings_.attack_seconds <= 0.0f) {
    value_ = 1.0f;
    stage_ = settings_.decay_seconds > 0.0f ? Stage::Decay : Stage::Sustain;
    if (stage_ == Stage::Sustain) {
      value_ = settings_.sustain_level;
    }
    return;
  }

  stage_ = Stage::Attack;
  if (value_ < 0.0f || value_ > 1.0f) {
    value_ = 0.0f;
  }
}

void EnvelopeGenerator::noteOff() {
  if (stage_ == Stage::Idle || stage_ == Stage::Release) {
    return;
  }

  release_start_level_ = value_;
  if (settings_.release_seconds <= 0.0f) {
    reset();
  } else {
    stage_ = Stage::Release;
  }
}

float EnvelopeGenerator::process(float delta_seconds) {
  const float dt = std::max(0.0f, delta_seconds);

  switch (stage_) {
    case Stage::Idle:
      value_ = 0.0f;
      break;
    case Stage::Attack:
      if (settings_.attack_seconds <= 0.0f) {
        value_ = 1.0f;
        stage_ = settings_.decay_seconds > 0.0f ? Stage::Decay : Stage::Sustain;
        if (stage_ == Stage::Sustain) {
          value_ = settings_.sustain_level;
        }
      } else {
        value_ += dt / settings_.attack_seconds;
        if (value_ >= 1.0f) {
          value_ = 1.0f;
          stage_ = settings_.decay_seconds > 0.0f ? Stage::Decay : Stage::Sustain;
          if (stage_ == Stage::Sustain) {
            value_ = settings_.sustain_level;
          }
        }
      }
      break;
    case Stage::Decay:
      if (settings_.decay_seconds <= 0.0f) {
        value_ = settings_.sustain_level;
        stage_ = Stage::Sustain;
      } else {
        const float step = (1.0f - settings_.sustain_level) * (dt / settings_.decay_seconds);
        value_ -= step;
        if (value_ <= settings_.sustain_level) {
          value_ = settings_.sustain_level;
          stage_ = Stage::Sustain;
        }
      }
      break;
    case Stage::Sustain:
      value_ = settings_.sustain_level;
      break;
    case Stage::Release:
      if (settings_.release_seconds <= 0.0f) {
        reset();
      } else {
        value_ -= (release_start_level_ * dt) / settings_.release_seconds;
        if (value_ <= 0.0f) {
          reset();
        }
      }
      break;
  }

  value_ = std::clamp(value_, 0.0f, 1.0f);
  return value_;
}

bool EnvelopeGenerator::isActive() const {
  return stage_ != Stage::Idle;
}

float EnvelopeGenerator::value() const {
  return value_;
}

EnvelopeGenerator::Stage EnvelopeGenerator::stage() const {
  return stage_;
}

