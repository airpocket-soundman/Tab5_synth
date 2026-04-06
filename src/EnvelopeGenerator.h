#pragma once

struct EnvelopeSettings {
  float attack_seconds = 0.02f;
  float decay_seconds = 0.18f;
  float sustain_level = 0.7f;
  float release_seconds = 0.5f;
};

class EnvelopeGenerator {
 public:
  enum class Stage {
    Idle,
    Attack,
    Decay,
    Sustain,
    Release,
  };

  void setSettings(const EnvelopeSettings& settings);
  void reset();
  void noteOn();
  void noteOff();
  float process(float delta_seconds);

  [[nodiscard]] bool isActive() const;
  [[nodiscard]] float value() const;
  [[nodiscard]] Stage stage() const;

 private:
  EnvelopeSettings settings_{};
  Stage stage_ = Stage::Idle;
  float value_ = 0.0f;
  float release_start_level_ = 0.0f;
};
