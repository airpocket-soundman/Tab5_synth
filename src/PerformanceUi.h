#pragma once

#include "AudioSourceType.h"
#include "ParameterIcon.h"
#include "Rect.h"
#include "SynthConfig.h"
#include "Waveform.h"

#include <M5Unified.h>

#include <array>
#include <cstddef>

enum class UiParameter {
  Volume,
  Attack,
  Decay,
  Sustain,
  Release,
};

struct UiState {
  Waveform selected_waveform = Waveform::Sine;
  AudioSourceType selected_source = AudioSourceType::Oscillator;
  UiParameter selected_parameter = UiParameter::Volume;
  bool quantize_to_semitone = false;
  bool oscillator_available = true;
  bool onboard_mic_available = false;
  bool external_i2s_available = false;
  bool note_playing = false;
  int active_midi_note = -1;
  float active_frequency = 0.0f;
  float volume = SynthConfig::audio.default_volume;
  float attack = SynthConfig::audio.amp_attack_default_ms / SynthConfig::audio.amp_attack_max_ms;
  float decay = SynthConfig::audio.amp_decay_default_ms / SynthConfig::audio.amp_decay_max_ms;
  float sustain = SynthConfig::audio.amp_sustain_default;
  float release = SynthConfig::audio.amp_release_default_ms / SynthConfig::audio.amp_release_max_ms;
  std::size_t touch_count = 0;
  std::array<int, SynthConfig::ui.max_touch_points> touch_xs{};
  std::array<int, SynthConfig::ui.max_touch_points> touch_ys{};
  std::size_t keyboard_note_count = 0;
  std::array<int, SynthConfig::ui.max_touch_points> keyboard_notes{};
};

class PerformanceUi {
 public:
  void begin();
  void drawInitial(const UiState& state);
  void refreshPerformance(const UiState& state);
  void refreshSourceSelection(const UiState& state);
  void refreshParameterSelection(const UiState& state);
  void refreshParameterControl(const UiState& state);
  void refreshPitchMode(const UiState& state);
  void refreshKeyboard(const UiState& state);

  [[nodiscard]] bool isSelectionArea(int x, int y) const;
  [[nodiscard]] bool isParameterArea(int x, int y) const;
  [[nodiscard]] bool isPerformanceArea(int x, int y) const;
  [[nodiscard]] bool isKeyboardArea(int x, int y) const;
  [[nodiscard]] bool isSliderArea(int x, int y) const;
  [[nodiscard]] bool isPitchModeArea(int x, int y) const;
  [[nodiscard]] bool quantizeModeAt(int x, int y, bool fallback) const;
  [[nodiscard]] Waveform waveformAt(int x, int y, Waveform fallback) const;
  [[nodiscard]] AudioSourceType sourceAt(int x, int y, AudioSourceType fallback) const;
  [[nodiscard]] UiParameter parameterAt(int x, int y, UiParameter fallback) const;
  [[nodiscard]] float xToNoteValue(int x, bool quantize_to_semitone) const;
  [[nodiscard]] float keyboardNoteValueAt(int x, int y) const;
  [[nodiscard]] float sliderValueFromTouch(int x) const;

 private:
  [[nodiscard]] bool isSourceAvailable(AudioSourceType source, const UiState& state) const;
  [[nodiscard]] float parameterValue(const UiState& state, UiParameter parameter) const;
  [[nodiscard]] const char* parameterLabel(UiParameter parameter) const;
  [[nodiscard]] bool isBlackKeySemitone(int semitone) const;
  [[nodiscard]] bool hasKeyboardNote(const UiState& state, int note) const;
  void layout();
  void drawSourceButtons(const UiState& state);
  void drawSourceButton(std::size_t index, const UiState& state);
  void drawParameterButtons(const UiState& state);
  void drawParameterButton(std::size_t index, const UiState& state);
  void drawWaveformIcon(const Rect& rect, Waveform waveform, std::uint32_t color);
  void drawSourceLabel(const Rect& rect, AudioSourceType source, std::uint32_t color);
  void drawSliderControl(const UiState& state);
  void drawPitchModeSwitch(const UiState& state);
  void drawPitchModeButton(const Rect& rect, const char* label, bool selected);
  void drawPerformanceBase();
  void drawKeyboardBase();
  void drawKeyboardOverlays(const UiState& state);
  void drawBlackKey(int octave, std::size_t black_index, bool active);
  void redrawBlackKeys(const UiState& state);
  void drawKeyboardNote(int note, bool active);
  void eraseTouchMarkers();
  void drawTouchMarkers(const UiState& state);
  void drawCenterGuides(const Rect& bounds);
  void eraseMarkerAt(int x, int y);
  void drawMarkerAt(int x, int y, std::uint32_t fill);

  std::array<Rect, 6> source_buttons_{};
  std::array<Rect, 5> parameter_buttons_{};
  Rect slider_area_{};
  Rect performance_area_{};
  Rect keyboard_area_{};
  Rect pitch_mode_area_{};
  Rect semitone_button_{};
  Rect continuous_button_{};
  std::array<ParameterIcon, 5> parameter_icons_{};
  std::size_t previous_touch_count_ = 0;
  std::array<int, SynthConfig::ui.max_touch_points> previous_touch_xs_{};
  std::array<int, SynthConfig::ui.max_touch_points> previous_touch_ys_{};
  std::size_t previous_keyboard_note_count_ = 0;
  std::array<int, SynthConfig::ui.max_touch_points> previous_keyboard_notes_{};
};

