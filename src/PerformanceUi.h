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
  DelayTime,
  DelayFeedback,
  DelayMix,
  ChorusRate,
  ChorusDepth,
  ChorusMix,
  FilterCutoff,
  FilterResonance,
  FilterMix,
  DistortionDrive,
  DistortionTone,
  DistortionMix,
  BitcrusherBits,
  BitcrusherRate,
  BitcrusherMix,
};

enum class UiEffect {
  Delay,
  Chorus,
  Filter,
  Distortion,
  Bitcrusher,
};

enum class UiLfoParameter { Rate, Depth, Shape };

struct UiState {
  Waveform selected_waveform = Waveform::Sine;
  AudioSourceType selected_source = AudioSourceType::Oscillator;
  UiParameter selected_parameter = UiParameter::Volume;
  UiEffect selected_effect = UiEffect::Delay;
  UiLfoParameter selected_lfo_parameter = UiLfoParameter::Rate;
  std::size_t selected_memory_slot = 0;
  std::size_t selected_preset = 0;
  bool preset_active = false;
  bool lfo_edit_mode = false;
  bool lfo_enabled = false;
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
  float delay_time = 0.35f;
  float delay_feedback = 0.40f;
  float delay_mix = 0.30f;
  bool delay_enabled = true;
  float chorus_rate = 0.30f;
  float chorus_depth = 0.40f;
  float chorus_mix = 0.30f;
  bool chorus_enabled = true;
  float filter_cutoff = 1.00f;
  float filter_resonance = 0.25f;
  float filter_mix = 0.45f;
  bool filter_enabled = false;
  float distortion_drive = 0.40f;
  float distortion_tone = 0.55f;
  float distortion_mix = 0.00f;
  bool distortion_enabled = false;
  float bitcrusher_bits = 1.00f;
  float bitcrusher_rate = 1.00f;
  float bitcrusher_mix = 0.00f;
  bool bitcrusher_enabled = false;
  float lfo_rate = 0.35f;
  float lfo_depth = 0.45f;
  float lfo_shape = 0.20f;
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
  [[nodiscard]] bool isEffectArea(int x, int y) const;
  [[nodiscard]] bool isLfoArea(int x, int y) const;
  [[nodiscard]] bool isLfoLabelArea(int x, int y) const;
  [[nodiscard]] bool isPerformanceArea(int x, int y) const;
  [[nodiscard]] bool isKeyboardArea(int x, int y) const;
  [[nodiscard]] bool isSliderArea(int x, int y) const;
  [[nodiscard]] bool isMemorySlotArea(int x, int y) const;
  [[nodiscard]] bool isPresetArea(int x, int y) const;
  [[nodiscard]] bool isPitchModeArea(int x, int y) const;
  [[nodiscard]] bool quantizeModeAt(int x, int y, bool fallback) const;
  [[nodiscard]] Waveform waveformAt(int x, int y, Waveform fallback) const;
  [[nodiscard]] AudioSourceType sourceAt(int x, int y, AudioSourceType fallback) const;
  [[nodiscard]] UiParameter parameterAt(int x, int y, UiParameter fallback) const;
  [[nodiscard]] UiEffect effectAt(int x, int y, UiEffect fallback) const;
  [[nodiscard]] UiLfoParameter lfoAt(int x, int y, UiLfoParameter fallback) const;
  [[nodiscard]] float xToNoteValue(int x, bool quantize_to_semitone) const;
  [[nodiscard]] float keyboardNoteValueAt(int x, int y) const;
  [[nodiscard]] float sliderValueFromTouch(int x) const;
  [[nodiscard]] std::size_t memorySlotAt(int x, int y, std::size_t fallback) const;
  [[nodiscard]] std::size_t presetAt(int x, int y, std::size_t fallback) const;

 private:
  [[nodiscard]] bool isSourceAvailable(AudioSourceType source, const UiState& state) const;
  [[nodiscard]] float parameterValue(const UiState& state, UiParameter parameter) const;
  [[nodiscard]] const char* parameterLabel(UiParameter parameter) const;
  [[nodiscard]] float lfoValue(const UiState& state, UiLfoParameter parameter) const;
  [[nodiscard]] const char* lfoLabel(UiLfoParameter parameter) const;
  [[nodiscard]] bool isBlackKeySemitone(int semitone) const;
  [[nodiscard]] bool hasKeyboardNote(const UiState& state, int note) const;
  void layout();
  void drawSourceButtons(const UiState& state);
  void drawSourceButton(std::size_t index, const UiState& state);
  void drawParameterButtons(const UiState& state);
  void drawParameterButton(std::size_t index, const UiState& state);
  void drawEffectIcons(const UiState& state);
  void drawEffectIcon(std::size_t index, const UiState& state);
  void drawDelayParameterIcons(const UiState& state);
  void drawDelayParameterIcon(std::size_t index, const UiState& state);
  void drawChorusParameterIcons(const UiState& state);
  void drawChorusParameterIcon(std::size_t index, const UiState& state);
  void drawFilterParameterIcons(const UiState& state);
  void drawFilterParameterIcon(std::size_t index, const UiState& state);
  void drawDistortionParameterIcons(const UiState& state);
  void drawDistortionParameterIcon(std::size_t index, const UiState& state);
  void drawBitcrusherParameterIcons(const UiState& state);
  void drawBitcrusherParameterIcon(std::size_t index, const UiState& state);
  void drawLfoIcons(const UiState& state);
  void drawLfoLabel(const UiState& state) const;
  void drawLfoIcon(std::size_t index, const UiState& state);
  void drawEnvelopePreview(const UiState& state);
  void drawWaveformIcon(const Rect& rect, Waveform waveform, std::uint32_t color);
  void drawSourceLabel(const Rect& rect, AudioSourceType source, std::uint32_t color);
  void drawSliderControl(const UiState& state);
  void drawMemorySlots(const UiState& state);
  void drawMemorySlot(std::size_t index, const UiState& state);
  void drawPresets(const UiState& state);
  void drawPreset(std::size_t index, const UiState& state);
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
  std::array<Rect, 4> effect_icon_buttons_{};
  std::array<Rect, 3> delay_parameter_buttons_{};
  std::array<Rect, 3> chorus_parameter_buttons_{};
  std::array<Rect, 2> filter_parameter_buttons_{};
  std::array<Rect, 3> distortion_parameter_buttons_{};
  std::array<Rect, 3> bitcrusher_parameter_buttons_{};
  Rect lfo_label_area_{};
  std::array<Rect, 3> lfo_icon_buttons_{};
  Rect envelope_preview_area_{};
  Rect slider_area_{};
  std::array<Rect, 11> preset_buttons_{};
  std::array<Rect, 5> memory_slot_buttons_{};
  Rect performance_area_{};
  Rect keyboard_area_{};
  Rect pitch_mode_area_{};
  Rect semitone_button_{};
  Rect continuous_button_{};
  std::array<ParameterIcon, 5> parameter_icons_{};
  std::array<ParameterIcon, 3> delay_parameter_icons_{};
  std::array<ParameterIcon, 3> chorus_parameter_icons_{};
  std::array<ParameterIcon, 2> filter_parameter_icons_{};
  std::array<ParameterIcon, 3> distortion_parameter_icons_{};
  std::array<ParameterIcon, 3> bitcrusher_parameter_icons_{};
  std::array<ParameterIcon, 3> lfo_parameter_icons_{};
  std::size_t previous_touch_count_ = 0;
  std::array<int, SynthConfig::ui.max_touch_points> previous_touch_xs_{};
  std::array<int, SynthConfig::ui.max_touch_points> previous_touch_ys_{};
  std::size_t previous_keyboard_note_count_ = 0;
  std::array<int, SynthConfig::ui.max_touch_points> previous_keyboard_notes_{};
};

