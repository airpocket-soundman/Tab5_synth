#pragma once

#include "AudioEngine.h"
#include "PerformanceUi.h"

#include <Preferences.h>

#include <array>
#include <cstdint>

class SynthApp {
 public:
  void begin();
  void update();

 private:
  void handleTouch();
  void handleSelectionTouch(int x, int y);
  void handlePitchModeTouch(int x, int y);
  void handleSliderTouch(int x, int y);
  void handlePerformanceTouches(const std::array<float, SynthConfig::ui.max_touch_points>& note_values,
                                std::size_t note_count,
                                const std::array<int, SynthConfig::ui.max_touch_points>& xs,
                                const std::array<int, SynthConfig::ui.max_touch_points>& ys,
                                std::size_t pad_count,
                                const std::array<int, SynthConfig::ui.max_touch_points>& keyboard_notes,
                                std::size_t keyboard_note_count);
  void finishMicRecording();
  void syncUiState();
  void stopNote();
  void loadLfoTargetState(UiParameter target);
  void storeLfoTargetState(UiParameter target);
  void saveCurrentSlot();
  void loadSlot(std::size_t slot_index);

  AudioEngine audio_engine_{};
  PerformanceUi ui_{};
  UiState ui_state_{};
  struct LfoTargetState {
    float rate = 0.35f;
    float depth = 0.45f;
    float shape = 0.20f;
    bool enabled = true;
  };
  static constexpr std::size_t kUiParameterCount = static_cast<std::size_t>(UiParameter::FilterMix) + 1;
  struct PersistedSlot {
    std::uint32_t version = 2;
    std::uint8_t selected_waveform = 0;
    std::uint8_t selected_source = 0;
    std::uint8_t selected_parameter = 0;
    std::uint8_t selected_effect = 0;
    std::uint8_t selected_lfo_parameter = 0;
    std::uint8_t quantize_to_semitone = 0;
    std::uint8_t delay_enabled = 1;
    std::uint8_t chorus_enabled = 1;
    std::uint8_t filter_enabled = 1;
    float volume = 0.45f;
    float attack = 0.0f;
    float decay = 0.0f;
    float sustain = 0.72f;
    float release = 0.0f;
    float delay_time = 0.35f;
    float delay_feedback = 0.40f;
    float delay_mix = 0.30f;
    float chorus_rate = 0.30f;
    float chorus_depth = 0.40f;
    float chorus_mix = 0.30f;
    float filter_cutoff = 0.70f;
    float filter_resonance = 0.25f;
    float filter_mix = 0.45f;
    float lfo_target_rate[kUiParameterCount]{};
    float lfo_target_depth[kUiParameterCount]{};
    float lfo_target_shape[kUiParameterCount]{};
    std::uint8_t lfo_target_enabled[kUiParameterCount]{};
  };
  std::array<LfoTargetState, kUiParameterCount> lfo_target_states_{};
  Preferences preferences_{};
  bool preferences_ready_ = false;
  bool mic_recording_ = false;
  std::uint32_t mic_recording_started_ms_ = 0;
  std::uint32_t last_note_input_ms_ = 0;
  std::uint32_t last_ui_refresh_ms_ = 0;
  std::size_t last_drawn_touch_count_ = 0;
  std::array<int, SynthConfig::ui.max_touch_points> last_drawn_touch_xs_{};
  std::array<int, SynthConfig::ui.max_touch_points> last_drawn_touch_ys_{};
  std::size_t last_drawn_keyboard_note_count_ = 0;
  std::array<int, SynthConfig::ui.max_touch_points> last_drawn_keyboard_notes_{};
};


