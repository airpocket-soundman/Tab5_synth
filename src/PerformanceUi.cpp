#include "PerformanceUi.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

constexpr int kWaveIconInsetX = 10;
constexpr int kWaveIconInsetTop = 18;
constexpr int kWaveIconInsetBottom = 18;
constexpr std::array<UiParameter, 5> kUiParameters = {
    UiParameter::Volume,
    UiParameter::Attack,
    UiParameter::Decay,
    UiParameter::Sustain,
    UiParameter::Release,
};
constexpr std::array<UiEffect, 4> kUiEffects = {
    UiEffect::Delay,
    UiEffect::Chorus,
    UiEffect::Distortion,
    UiEffect::Bitcrusher,
};
constexpr std::array<UiLfoParameter, 3> kUiLfoParameters = {
    UiLfoParameter::Rate,
    UiLfoParameter::Depth,
    UiLfoParameter::Shape,
};
constexpr std::array<const char*, 11> kPresetLabels = {
    "GTR", "PNO", "ORG", "REC", "PAD", "PLK", "BEL", "BRS", "BAS", "SYN", "RND",
};
constexpr std::array<int, 7> kWhiteSemitones = {0, 2, 4, 5, 7, 9, 11};
constexpr std::array<int, 5> kBlackSemitones = {1, 3, 6, 8, 10};
constexpr std::array<int, 5> kBlackWhiteSlot = {0, 1, 3, 4, 5};

bool isWaveformIndex(std::size_t index) {
  return index < static_cast<std::size_t>(Waveform::Count);
}

Waveform waveformFromIndex(std::size_t index) {
  return static_cast<Waveform>(index);
}

AudioSourceType sourceFromIndex(std::size_t index) {
  return index < static_cast<std::size_t>(Waveform::Count)
             ? AudioSourceType::Oscillator
             : static_cast<AudioSourceType>(index - static_cast<std::size_t>(Waveform::Count) + 1);
}

}  // namespace

void PerformanceUi::begin() {
  layout();
}

void PerformanceUi::drawInitial(const UiState& state) {
  M5.Display.fillScreen(SynthConfig::ui.background_color);
  drawSourceButtons(state);
  drawParameterButtons(state);
  drawPerformanceBase();
  drawPitchModeSwitch(state);
  drawSliderControl(state);
  drawPresets(state);
  drawMemorySlots(state);
  drawKeyboardBase();
  drawKeyboardOverlays(state);
  previous_keyboard_note_count_ = state.keyboard_note_count;
  previous_keyboard_notes_ = state.keyboard_notes;
  previous_touch_count_ = 0;
  previous_touch_xs_.fill(0);
  previous_touch_ys_.fill(0);
  drawTouchMarkers(state);
}

void PerformanceUi::refreshPerformance(const UiState& state) {
  eraseTouchMarkers();
  drawTouchMarkers(state);
}

void PerformanceUi::refreshKeyboard(const UiState& state) {
  bool white_key_changed = false;

  for (std::size_t i = 0; i < previous_keyboard_note_count_; ++i) {
    const int note = previous_keyboard_notes_[i];
    if (!hasKeyboardNote(state, note)) {
      drawKeyboardNote(note, false);
      if (!isBlackKeySemitone(note - SynthConfig::ui.keyboard_root_note)) {
        white_key_changed = true;
      }
    }
  }

  for (std::size_t i = 0; i < state.keyboard_note_count; ++i) {
    const int note = state.keyboard_notes[i];
    bool was_active = false;
    for (std::size_t j = 0; j < previous_keyboard_note_count_; ++j) {
      if (previous_keyboard_notes_[j] == note) {
        was_active = true;
        break;
      }
    }
    if (!was_active) {
      drawKeyboardNote(note, true);
      if (!isBlackKeySemitone(note - SynthConfig::ui.keyboard_root_note)) {
        white_key_changed = true;
      }
    }
  }

  if (white_key_changed) {
    redrawBlackKeys(state);
  }

  previous_keyboard_note_count_ = state.keyboard_note_count;
  previous_keyboard_notes_ = state.keyboard_notes;
}

void PerformanceUi::refreshSourceSelection(const UiState& state) {
  drawSourceButtons(state);
}

void PerformanceUi::refreshParameterSelection(const UiState& state) {
  drawParameterButtons(state);
  drawPresets(state);
  drawMemorySlots(state);
}

void PerformanceUi::refreshParameterControl(const UiState& state) {
  drawParameterButtons(state);
  drawSliderControl(state);
  drawPresets(state);
  drawMemorySlots(state);
}

void PerformanceUi::refreshPitchMode(const UiState& state) {
  drawPitchModeSwitch(state);
}

bool PerformanceUi::isSelectionArea(int x, int y) const {
  for (const auto& rect : source_buttons_) {
    if (rect.contains(x, y)) {
      return true;
    }
  }
  return false;
}

bool PerformanceUi::isParameterArea(int x, int y) const {
  for (const auto& icon : parameter_icons_) {
    if (icon.contains(x, y)) {
      return true;
    }
  }
  for (const auto& rect : delay_parameter_buttons_) {
    if (rect.contains(x, y)) {
      return true;
    }
  }
  for (const auto& rect : chorus_parameter_buttons_) {
    if (rect.contains(x, y)) {
      return true;
    }
  }
  for (const auto& rect : distortion_parameter_buttons_) {
    if (rect.contains(x, y)) {
      return true;
    }
  }
  for (const auto& rect : bitcrusher_parameter_buttons_) {
    if (rect.contains(x, y)) {
      return true;
    }
  }
  return false;
}

bool PerformanceUi::isEffectArea(int x, int y) const {
  for (const auto& rect : effect_icon_buttons_) {
    if (rect.contains(x, y)) {
      return true;
    }
  }
  return false;
}

bool PerformanceUi::isLfoArea(int x, int y) const {
  for (const auto& rect : lfo_icon_buttons_) {
    if (rect.contains(x, y)) {
      return true;
    }
  }
  return false;
}

bool PerformanceUi::isLfoLabelArea(int x, int y) const {
  return lfo_label_area_.contains(x, y);
}

bool PerformanceUi::isPerformanceArea(int x, int y) const {
  return performance_area_.contains(x, y);
}

bool PerformanceUi::isKeyboardArea(int x, int y) const {
  return keyboard_area_.contains(x, y);
}

bool PerformanceUi::isSliderArea(int x, int y) const {
  return slider_area_.contains(x, y);
}

bool PerformanceUi::isMemorySlotArea(int x, int y) const {
  for (const auto& rect : memory_slot_buttons_) {
    if (rect.contains(x, y)) {
      return true;
    }
  }
  return false;
}

bool PerformanceUi::isPresetArea(int x, int y) const {
  for (const auto& rect : preset_buttons_) {
    if (rect.contains(x, y)) {
      return true;
    }
  }
  return false;
}

bool PerformanceUi::isPitchModeArea(int x, int y) const {
  return pitch_mode_area_.contains(x, y);
}

bool PerformanceUi::quantizeModeAt(int x, int y, bool fallback) const {
  if (semitone_button_.contains(x, y)) {
    return true;
  }
  if (continuous_button_.contains(x, y)) {
    return false;
  }
  return fallback;
}

Waveform PerformanceUi::waveformAt(int x, int y, Waveform fallback) const {
  for (std::size_t i = 0; i < source_buttons_.size(); ++i) {
    if (!isWaveformIndex(i)) {
      continue;
    }
    if (source_buttons_[i].contains(x, y)) {
      return waveformFromIndex(i);
    }
  }
  return fallback;
}

AudioSourceType PerformanceUi::sourceAt(int x, int y, AudioSourceType fallback) const {
  for (std::size_t i = 0; i < source_buttons_.size(); ++i) {
    if (source_buttons_[i].contains(x, y)) {
      return sourceFromIndex(i);
    }
  }
  return fallback;
}

UiParameter PerformanceUi::parameterAt(int x, int y, UiParameter fallback) const {
  for (std::size_t i = 0; i < parameter_buttons_.size(); ++i) {
    if (parameter_buttons_[i].contains(x, y)) {
      return kUiParameters[i];
    }
  }
  for (std::size_t i = 0; i < delay_parameter_buttons_.size(); ++i) {
    if (delay_parameter_buttons_[i].contains(x, y)) {
      switch (i) {
        case 0:
          return UiParameter::DelayTime;
        case 1:
          return UiParameter::DelayFeedback;
        case 2:
          return UiParameter::DelayMix;
      }
    }
  }
  for (std::size_t i = 0; i < chorus_parameter_buttons_.size(); ++i) {
    if (chorus_parameter_buttons_[i].contains(x, y)) {
      switch (i) {
        case 0:
          return UiParameter::ChorusRate;
        case 1:
          return UiParameter::ChorusDepth;
        case 2:
          return UiParameter::ChorusMix;
      }
    }
  }
  for (std::size_t i = 0; i < distortion_parameter_buttons_.size(); ++i) {
    if (distortion_parameter_buttons_[i].contains(x, y)) {
      switch (i) {
        case 0:
          return UiParameter::DistortionDrive;
        case 1:
          return UiParameter::DistortionTone;
        case 2:
          return UiParameter::DistortionMix;
      }
    }
  }
  for (std::size_t i = 0; i < bitcrusher_parameter_buttons_.size(); ++i) {
    if (bitcrusher_parameter_buttons_[i].contains(x, y)) {
      switch (i) {
        case 0:
          return UiParameter::BitcrusherBits;
        case 1:
          return UiParameter::BitcrusherRate;
        case 2:
          return UiParameter::BitcrusherMix;
      }
    }
  }
  return fallback;
}

UiEffect PerformanceUi::effectAt(int x, int y, UiEffect fallback) const {
  for (std::size_t i = 0; i < effect_icon_buttons_.size(); ++i) {
    if (effect_icon_buttons_[i].contains(x, y)) {
      return kUiEffects[i];
    }
  }
  return fallback;
}

UiLfoParameter PerformanceUi::lfoAt(int x, int y, UiLfoParameter fallback) const {
  for (std::size_t i = 0; i < lfo_icon_buttons_.size(); ++i) {
    if (lfo_icon_buttons_[i].contains(x, y)) {
      return kUiLfoParameters[i];
    }
  }
  return fallback;
}

float PerformanceUi::xToNoteValue(int x, bool quantize_to_semitone) const {
  const int clamped_x = std::clamp(x, performance_area_.x, performance_area_.x + performance_area_.w - 1);
  const float normalized = static_cast<float>(clamped_x - performance_area_.x) /
                           static_cast<float>(std::max(1, performance_area_.w - 1));
  const float note_span = static_cast<float>(SynthConfig::ui.max_midi_note - SynthConfig::ui.min_midi_note);
  const float note_value = static_cast<float>(SynthConfig::ui.min_midi_note) + (normalized * note_span);
  return quantize_to_semitone ? std::round(note_value) : note_value;
}

float PerformanceUi::keyboardNoteValueAt(int x, int y) const {
  const int octaves = std::max(1, SynthConfig::ui.keyboard_octaves);
  const int total_white = octaves * 7 + 1;
  const float white_w = static_cast<float>(keyboard_area_.w) / static_cast<float>(total_white);
  const float black_w = white_w * SynthConfig::ui.keyboard_black_width_ratio;
  const int black_h = static_cast<int>(std::round(keyboard_area_.h * SynthConfig::ui.keyboard_black_height_ratio));
  const int local_x = x - keyboard_area_.x;
  const int local_y = y - keyboard_area_.y;

  if (local_y >= 0 && local_y < black_h) {
    for (int octave = 0; octave < octaves; ++octave) {
      for (std::size_t i = 0; i < kBlackSemitones.size(); ++i) {
        const float center = (static_cast<float>(octave * 7 + kBlackWhiteSlot[i] + 1) * white_w);
        const int left = static_cast<int>(std::round(center - (black_w * 0.5f)));
        const int right = static_cast<int>(std::round(center + (black_w * 0.5f)));
        if (local_x >= left && local_x < right) {
          return static_cast<float>(SynthConfig::ui.keyboard_root_note + octave * 12 + kBlackSemitones[i]);
        }
      }
    }
  }

  const int white_index = std::clamp(static_cast<int>(std::floor(local_x / std::max(1.0f, white_w))), 0, total_white - 1);
  if (white_index == total_white - 1) {
    return static_cast<float>(SynthConfig::ui.keyboard_root_note + octaves * 12);
  }
  const int octave = white_index / 7;
  const int step = white_index % 7;
  return static_cast<float>(SynthConfig::ui.keyboard_root_note + octave * 12 + kWhiteSemitones[step]);
}

float PerformanceUi::sliderValueFromTouch(int x) const {
  const int slider_left = slider_area_.x + SynthConfig::ui.slider_inset;
  const int slider_right = slider_area_.x + slider_area_.w - SynthConfig::ui.slider_inset;
  const int clamped_x = std::clamp(x, slider_left, slider_right);
  return static_cast<float>(clamped_x - slider_left) /
         static_cast<float>(std::max(1, slider_right - slider_left));
}

bool PerformanceUi::isSourceAvailable(AudioSourceType source, const UiState& state) const {
  switch (source) {
    case AudioSourceType::Oscillator:
      return state.oscillator_available;
    case AudioSourceType::OnboardMic:
      return state.onboard_mic_available;
    case AudioSourceType::ExternalI2S:
      return state.external_i2s_available;
    default:
      return false;
  }
}

float PerformanceUi::parameterValue(const UiState& state, UiParameter parameter) const {
  switch (parameter) {
    case UiParameter::Volume:
      return state.volume;
    case UiParameter::Attack:
      return state.attack;
    case UiParameter::Decay:
      return state.decay;
    case UiParameter::Sustain:
      return state.sustain;
    case UiParameter::Release:
      return state.release;
    case UiParameter::DelayTime:
      return state.delay_time;
    case UiParameter::DelayFeedback:
      return state.delay_feedback;
    case UiParameter::DelayMix:
      return state.delay_mix;
    case UiParameter::ChorusRate:
      return state.chorus_rate;
    case UiParameter::ChorusDepth:
      return state.chorus_depth;
    case UiParameter::ChorusMix:
      return state.chorus_mix;
    case UiParameter::FilterCutoff:
      return state.filter_cutoff;
    case UiParameter::FilterResonance:
      return state.filter_resonance;
    case UiParameter::FilterMix:
      return state.filter_mix;
    case UiParameter::DistortionDrive:
      return state.distortion_drive;
    case UiParameter::DistortionTone:
      return state.distortion_tone;
    case UiParameter::DistortionMix:
      return state.distortion_mix;
    case UiParameter::BitcrusherBits:
      return state.bitcrusher_bits;
    case UiParameter::BitcrusherRate:
      return state.bitcrusher_rate;
    case UiParameter::BitcrusherMix:
      return state.bitcrusher_mix;
    default:
      return state.volume;
  }
}

std::size_t PerformanceUi::memorySlotAt(int x, int y, std::size_t fallback) const {
  for (std::size_t i = 0; i < memory_slot_buttons_.size(); ++i) {
    if (memory_slot_buttons_[i].contains(x, y)) {
      return i;
    }
  }
  return fallback;
}

std::size_t PerformanceUi::presetAt(int x, int y, std::size_t fallback) const {
  for (std::size_t i = 0; i < preset_buttons_.size(); ++i) {
    if (preset_buttons_[i].contains(x, y)) {
      return i;
    }
  }
  return fallback;
}

const char* PerformanceUi::parameterLabel(UiParameter parameter) const {
  switch (parameter) {
    case UiParameter::Volume:
      return "VOL";
    case UiParameter::Attack:
      return "ATK";
    case UiParameter::Decay:
      return "DEC";
    case UiParameter::Sustain:
      return "SUS";
    case UiParameter::Release:
      return "REL";
    case UiParameter::DelayTime:
      return "TIME";
    case UiParameter::DelayFeedback:
      return "FBK";
    case UiParameter::DelayMix:
      return "MIX";
    case UiParameter::ChorusRate:
      return "RATE";
    case UiParameter::ChorusDepth:
      return "DEP";
    case UiParameter::ChorusMix:
      return "MIX";
    case UiParameter::FilterCutoff:
      return "CUT";
    case UiParameter::FilterResonance:
      return "RES";
    case UiParameter::FilterMix:
      return "MIX";
    case UiParameter::DistortionDrive:
      return "DRV";
    case UiParameter::DistortionTone:
      return "TON";
    case UiParameter::DistortionMix:
      return "MIX";
    case UiParameter::BitcrusherBits:
      return "BITS";
    case UiParameter::BitcrusherRate:
      return "RATE";
    case UiParameter::BitcrusherMix:
      return "MIX";
    default:
      return "VOL";
  }
}

float PerformanceUi::lfoValue(const UiState& state, UiLfoParameter parameter) const {
  switch (parameter) {
    case UiLfoParameter::Rate:
      return state.lfo_rate;
    case UiLfoParameter::Depth:
      return state.lfo_depth;
    case UiLfoParameter::Shape:
      return state.lfo_shape;
    default:
      return state.lfo_rate;
  }
}

const char* PerformanceUi::lfoLabel(UiLfoParameter parameter) const {
  switch (parameter) {
    case UiLfoParameter::Rate:
      return "RAT";
    case UiLfoParameter::Depth:
      return "DEP";
    case UiLfoParameter::Shape:
      return "WAV";
    default:
      return "RAT";
  }
}

bool PerformanceUi::isBlackKeySemitone(int semitone) const {
  switch (semitone % 12) {
    case 1:
    case 3:
    case 6:
    case 8:
    case 10:
      return true;
    default:
      return false;
  }
}

bool PerformanceUi::hasKeyboardNote(const UiState& state, int note) const {
  for (std::size_t i = 0; i < state.keyboard_note_count; ++i) {
    if (state.keyboard_notes[i] == note) {
      return true;
    }
  }
  return false;
}

void PerformanceUi::layout() {
  const int width = M5.Display.width();
  const int height = M5.Display.height();
  const int right_zone_width = std::clamp(
      static_cast<int>(std::round(width * SynthConfig::ui.performance_area_width_ratio)), width / 4, width - 160);
  const int left_width = width - right_zone_width;

  const int source_button_width =
      (left_width - (SynthConfig::ui.wave_button_padding * 2) -
       (SynthConfig::ui.selection_button_gap * (static_cast<int>(source_buttons_.size()) - 1))) /
      static_cast<int>(source_buttons_.size());

  for (std::size_t i = 0; i < source_buttons_.size(); ++i) {
    const int x = SynthConfig::ui.wave_button_padding +
                  static_cast<int>(i) * (source_button_width + SynthConfig::ui.selection_button_gap);
    source_buttons_[i] = {x, SynthConfig::ui.wave_button_padding, source_button_width,
                          SynthConfig::ui.selection_button_height};
  }

  const int parameter_row_y = SynthConfig::ui.wave_button_padding + SynthConfig::ui.selection_button_height +
                              SynthConfig::ui.parameter_button_gap;
  // Keep EG parameter icon width aligned with the VCO/source button width.
  const int parameter_button_width = source_button_width;
  const int parameter_row_x = SynthConfig::ui.wave_button_padding;

  for (std::size_t i = 0; i < parameter_buttons_.size(); ++i) {
    const int x = parameter_row_x +
                  static_cast<int>(i) * (parameter_button_width + SynthConfig::ui.parameter_button_gap);
    parameter_buttons_[i] = {x, parameter_row_y, parameter_button_width, SynthConfig::ui.parameter_button_height};
    parameter_icons_[i].begin(parameter_buttons_[i]);
  }
  const int preview_x = parameter_row_x +
                        static_cast<int>(parameter_buttons_.size()) *
                            (parameter_button_width + SynthConfig::ui.parameter_button_gap);
  envelope_preview_area_ = {preview_x, parameter_row_y, parameter_button_width, SynthConfig::ui.parameter_button_height};

  const int effect_h = source_buttons_[0].h;
  const int effect_w = std::max(24, source_buttons_[0].w / 2);
  const int effect_gap = SynthConfig::ui.selection_button_gap;
  const int effect_param_w = source_buttons_[0].w;
  const int effects_x = source_buttons_.back().x + source_buttons_.back().w + effect_gap;
  const int effects_y = source_buttons_[0].y;

  effect_icon_buttons_[0] = {effects_x, effects_y, effect_w, effect_h};  // DLY label
  for (std::size_t i = 0; i < delay_parameter_buttons_.size(); ++i) {
    delay_parameter_buttons_[i] = {effect_icon_buttons_[0].x + effect_w + effect_gap +
                                       static_cast<int>(i) * (effect_param_w + effect_gap),
                                   effect_icon_buttons_[0].y,
                                   effect_param_w,
                                   effect_h};
    delay_parameter_icons_[i].begin(delay_parameter_buttons_[i]);
  }

  effect_icon_buttons_[1] = {effects_x, effects_y + effect_h + effect_gap, effect_w, effect_h};  // CHR label
  for (std::size_t i = 0; i < chorus_parameter_buttons_.size(); ++i) {
    chorus_parameter_buttons_[i] = {effect_icon_buttons_[1].x + effect_w + effect_gap +
                                        static_cast<int>(i) * (effect_param_w + effect_gap),
                                    effect_icon_buttons_[1].y,
                                    effect_param_w,
                                    effect_h};
    chorus_parameter_icons_[i].begin(chorus_parameter_buttons_[i]);
  }

  effect_icon_buttons_[2] = {effects_x, effects_y + (effect_h + effect_gap) * 2, effect_w, effect_h};  // DST label
  for (std::size_t i = 0; i < distortion_parameter_buttons_.size(); ++i) {
    distortion_parameter_buttons_[i] = {effect_icon_buttons_[2].x + effect_w + effect_gap +
                                            static_cast<int>(i) * (effect_param_w + effect_gap),
                                        effect_icon_buttons_[2].y,
                                        effect_param_w,
                                        effect_h};
    distortion_parameter_icons_[i].begin(distortion_parameter_buttons_[i]);
  }

  effect_icon_buttons_[3] = {effects_x, effects_y + (effect_h + effect_gap) * 3, effect_w, effect_h};  // BFR label
  for (std::size_t i = 0; i < bitcrusher_parameter_buttons_.size(); ++i) {
    bitcrusher_parameter_buttons_[i] = {effect_icon_buttons_[3].x + effect_w + effect_gap +
                                            static_cast<int>(i) * (effect_param_w + effect_gap),
                                        effect_icon_buttons_[3].y,
                                        effect_param_w,
                                        effect_h};
    bitcrusher_parameter_icons_[i].begin(bitcrusher_parameter_buttons_[i]);
  }

  const int lfo_y = parameter_row_y + SynthConfig::ui.parameter_button_height + SynthConfig::ui.parameter_button_gap;
  const int lfo_label_w = std::max(24, source_buttons_[0].w / 2);
  const int lfo_h = SynthConfig::ui.parameter_button_height;
  lfo_label_area_ = {SynthConfig::ui.wave_button_padding, lfo_y, lfo_label_w, lfo_h};
  for (std::size_t i = 0; i < lfo_icon_buttons_.size(); ++i) {
    const int x = lfo_label_area_.x + lfo_label_area_.w + SynthConfig::ui.parameter_button_gap +
                  static_cast<int>(i) * (parameter_button_width + SynthConfig::ui.parameter_button_gap);
    lfo_icon_buttons_[i] = {x, lfo_y, parameter_button_width, lfo_h};
    lfo_parameter_icons_[i].begin(lfo_icon_buttons_[i]);
  }

  const int right_zone_x = left_width;
  const int square_limit = std::min(right_zone_width - (SynthConfig::ui.wave_button_padding * 2),
                                    height - (SynthConfig::ui.wave_button_padding * 2) -
                                        (SynthConfig::ui.pitch_mode_height * 2) -
                                        SynthConfig::ui.pitch_mode_gap * 2 -
                                        SynthConfig::ui.keyboard_min_height - SynthConfig::ui.keyboard_top_gap -
                                        SynthConfig::ui.keyboard_bottom_margin);
  const int square_size =
      std::max(180, static_cast<int>(std::round(square_limit * SynthConfig::ui.performance_square_scale)));
  const int square_x = right_zone_x + right_zone_width - square_size - SynthConfig::ui.wave_button_padding;
  const int square_y = SynthConfig::ui.performance_square_top_margin;
  performance_area_ = {square_x, square_y, square_size, square_size};

  const int pitch_y = performance_area_.y + performance_area_.h + SynthConfig::ui.pitch_mode_gap;
  pitch_mode_area_ = {performance_area_.x, pitch_y, performance_area_.w, SynthConfig::ui.pitch_mode_height};

  const int mode_button_width = (pitch_mode_area_.w - SynthConfig::ui.pitch_mode_button_gap) / 2;
  semitone_button_ = {pitch_mode_area_.x, pitch_mode_area_.y, mode_button_width, pitch_mode_area_.h};
  continuous_button_ = {pitch_mode_area_.x + mode_button_width + SynthConfig::ui.pitch_mode_button_gap,
                        pitch_mode_area_.y, mode_button_width, pitch_mode_area_.h};

  const int slider_y = pitch_mode_area_.y + pitch_mode_area_.h + SynthConfig::ui.pitch_mode_gap;
  slider_area_ = {pitch_mode_area_.x, slider_y, pitch_mode_area_.w, SynthConfig::ui.pitch_mode_height};

  const int memory_y = slider_area_.y + slider_area_.h + SynthConfig::ui.pitch_mode_gap;
  const int memory_gap = 8;
  const int memory_w = (slider_area_.w - memory_gap * (static_cast<int>(memory_slot_buttons_.size()) - 1)) /
                       static_cast<int>(memory_slot_buttons_.size());
  for (std::size_t i = 0; i < memory_slot_buttons_.size(); ++i) {
    const int x = slider_area_.x + static_cast<int>(i) * (memory_w + memory_gap);
    memory_slot_buttons_[i] = {x, memory_y, memory_w, SynthConfig::ui.pitch_mode_height - 8};
  }

  const int preset_gap = 6;
  const int preset_left = SynthConfig::ui.wave_button_padding;
  const int preset_right = memory_slot_buttons_.front().x - memory_gap;
  const int preset_available = std::max(0, preset_right - preset_left);
  const int preset_w = (preset_available - preset_gap * (static_cast<int>(preset_buttons_.size()) - 1)) /
                       static_cast<int>(preset_buttons_.size());
  const int preset_h = std::max(20, SynthConfig::ui.pitch_mode_height - 8);
  for (std::size_t i = 0; i < preset_buttons_.size(); ++i) {
    const int x = preset_left + static_cast<int>(i) * (std::max(0, preset_w) + preset_gap);
    preset_buttons_[i] = {x, memory_y, std::max(0, preset_w), preset_h};
  }

  const int memory_bottom = memory_slot_buttons_.back().y + memory_slot_buttons_.back().h;
  const int keyboard_top = std::clamp(memory_bottom + SynthConfig::ui.keyboard_top_gap, 0, height);
  const int keyboard_bottom = std::clamp(height - SynthConfig::ui.keyboard_bottom_margin, keyboard_top, height);
  const int keyboard_height = std::max(0, keyboard_bottom - keyboard_top);
  keyboard_area_ = {SynthConfig::ui.keyboard_side_margin, keyboard_top,
                    width - (SynthConfig::ui.keyboard_side_margin * 2), keyboard_height};
}

void PerformanceUi::drawSourceButtons(const UiState& state) {
  for (std::size_t i = 0; i < source_buttons_.size(); ++i) {
    drawSourceButton(i, state);
  }
}

void PerformanceUi::drawSourceButton(std::size_t index, const UiState& state) {
  const auto& rect = source_buttons_[index];
  const bool waveform_slot = isWaveformIndex(index);
  const Waveform waveform = waveform_slot ? waveformFromIndex(index) : Waveform::Sine;
  const AudioSourceType source = sourceFromIndex(index);
  const bool selected = waveform_slot
                            ? (state.selected_source == AudioSourceType::Oscillator && state.selected_waveform == waveform)
                            : (state.selected_source == source);
  const bool available = isSourceAvailable(source, state);

  const std::uint32_t fill = available
                                 ? (selected ? (waveform_slot ? waveformColor(waveform) : SynthConfig::ui.slider_fill_color)
                                             : SynthConfig::ui.muted_button_color)
                                 : SynthConfig::ui.disabled_button_color;
  const std::uint32_t border = selected ? SynthConfig::ui.selected_border_color : SynthConfig::ui.muted_border_color;
  const std::uint32_t fg = available ? (selected ? SynthConfig::ui.selected_text_color : SynthConfig::ui.muted_text_color)
                                     : SynthConfig::ui.disabled_text_color;

  M5.Display.fillRoundRect(rect.x, rect.y, rect.w, rect.h, SynthConfig::ui.round_radius, fill);
  M5.Display.drawRoundRect(rect.x, rect.y, rect.w, rect.h, SynthConfig::ui.round_radius, border);
  M5.Display.drawRoundRect(rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2, SynthConfig::ui.round_radius, border);

  if (waveform_slot) {
    drawWaveformIcon(rect, waveform, fg);
  } else {
    drawSourceLabel(rect, source, fg);
  }
}

void PerformanceUi::drawParameterButtons(const UiState& state) {
  for (std::size_t i = 0; i < parameter_buttons_.size(); ++i) {
    drawParameterButton(i, state);
  }
  drawEffectIcons(state);
  drawDelayParameterIcons(state);
  drawChorusParameterIcons(state);
  drawDistortionParameterIcons(state);
  drawBitcrusherParameterIcons(state);
  drawLfoIcons(state);
  drawEnvelopePreview(state);
}

void PerformanceUi::drawParameterButton(std::size_t index, const UiState& state) {
  const UiParameter parameter = kUiParameters[index];
  parameter_icons_[index].drawBar(parameterLabel(parameter), parameterValue(state, parameter),
                                  state.selected_parameter == parameter);
}

void PerformanceUi::drawEffectIcons(const UiState& state) {
  for (std::size_t i = 0; i < effect_icon_buttons_.size(); ++i) {
    drawEffectIcon(i, state);
  }
}

void PerformanceUi::drawEffectIcon(std::size_t index, const UiState& state) {
  const Rect& rect = effect_icon_buttons_[index];
  const UiEffect effect = kUiEffects[index];
  const bool enabled = (effect == UiEffect::Delay)
                           ? state.delay_enabled
                           : (effect == UiEffect::Chorus) ? state.chorus_enabled
                                                           : (effect == UiEffect::Distortion) ? state.distortion_enabled
                                                                                               : state.bitcrusher_enabled;
  const std::uint32_t fill = enabled ? SynthConfig::ui.slider_fill_color : SynthConfig::ui.muted_button_color;
  const std::uint32_t border = enabled ? SynthConfig::ui.selected_border_color : SynthConfig::ui.muted_border_color;
  const std::uint32_t text = enabled ? SynthConfig::ui.selected_text_color : SynthConfig::ui.disabled_text_color;
  const char* label = (effect == UiEffect::Delay)
                          ? "DLY"
                          : (effect == UiEffect::Chorus) ? "CHR" : (effect == UiEffect::Distortion) ? "DST" : "BFR";

  M5.Display.fillRoundRect(rect.x, rect.y, rect.w, rect.h, SynthConfig::ui.round_radius, fill);
  M5.Display.drawRoundRect(rect.x, rect.y, rect.w, rect.h, SynthConfig::ui.round_radius, border);
  M5.Display.setTextColor(text);
  M5.Display.setTextSize(1);
  M5.Display.setTextDatum(middle_center);
  const int cx = rect.x + rect.w / 2;
  const int y0 = rect.y + rect.h / 2 - 24;
  char ch[2] = {0, 0};
  ch[0] = label[0];
  M5.Display.drawString(ch, cx, y0);
  ch[0] = label[1];
  M5.Display.drawString(ch, cx, y0 + 24);
  ch[0] = label[2];
  M5.Display.drawString(ch, cx, y0 + 48);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(2);
}

void PerformanceUi::drawDelayParameterIcons(const UiState& state) {
  for (std::size_t i = 0; i < delay_parameter_buttons_.size(); ++i) {
    drawDelayParameterIcon(i, state);
  }
}

void PerformanceUi::drawDelayParameterIcon(std::size_t index, const UiState& state) {
  const UiParameter parameter = (index == 0)
                                    ? UiParameter::DelayTime
                                    : (index == 1) ? UiParameter::DelayFeedback : UiParameter::DelayMix;
  const bool selected = (state.selected_parameter == parameter);
  const float value = parameterValue(state, parameter);
  const char* label = parameterLabel(parameter);
  delay_parameter_icons_[index].drawBar(label, value, selected);
}

void PerformanceUi::drawChorusParameterIcons(const UiState& state) {
  for (std::size_t i = 0; i < chorus_parameter_buttons_.size(); ++i) {
    drawChorusParameterIcon(i, state);
  }
}

void PerformanceUi::drawChorusParameterIcon(std::size_t index, const UiState& state) {
  const UiParameter parameter = (index == 0)
                                    ? UiParameter::ChorusRate
                                    : (index == 1) ? UiParameter::ChorusDepth : UiParameter::ChorusMix;
  const bool selected = (state.selected_parameter == parameter);
  const float value = parameterValue(state, parameter);
  const char* label = parameterLabel(parameter);
  chorus_parameter_icons_[index].drawBar(label, value, selected);
}

void PerformanceUi::drawFilterParameterIcons(const UiState& state) {
  for (std::size_t i = 0; i < filter_parameter_buttons_.size(); ++i) {
    drawFilterParameterIcon(i, state);
  }
}

void PerformanceUi::drawFilterParameterIcon(std::size_t index, const UiState& state) {
  const UiParameter parameter = (index == 0) ? UiParameter::FilterResonance : UiParameter::FilterMix;
  const bool selected = (state.selected_parameter == parameter);
  const float value = parameterValue(state, parameter);
  const char* label = parameterLabel(parameter);
  filter_parameter_icons_[index].drawBar(label, value, selected);
}

void PerformanceUi::drawDistortionParameterIcons(const UiState& state) {
  for (std::size_t i = 0; i < distortion_parameter_buttons_.size(); ++i) {
    drawDistortionParameterIcon(i, state);
  }
}

void PerformanceUi::drawDistortionParameterIcon(std::size_t index, const UiState& state) {
  const UiParameter parameter = (index == 0)
                                    ? UiParameter::DistortionDrive
                                    : (index == 1) ? UiParameter::DistortionTone : UiParameter::DistortionMix;
  const bool selected = (state.selected_parameter == parameter);
  const float value = parameterValue(state, parameter);
  const char* label = parameterLabel(parameter);
  distortion_parameter_icons_[index].drawBar(label, value, selected);
}

void PerformanceUi::drawBitcrusherParameterIcons(const UiState& state) {
  for (std::size_t i = 0; i < bitcrusher_parameter_buttons_.size(); ++i) {
    drawBitcrusherParameterIcon(i, state);
  }
}

void PerformanceUi::drawBitcrusherParameterIcon(std::size_t index, const UiState& state) {
  const UiParameter parameter = (index == 0)
                                    ? UiParameter::BitcrusherBits
                                    : (index == 1) ? UiParameter::BitcrusherRate : UiParameter::BitcrusherMix;
  const bool selected = (state.selected_parameter == parameter);
  const float value = parameterValue(state, parameter);
  const char* label = parameterLabel(parameter);
  bitcrusher_parameter_icons_[index].drawBar(label, value, selected);
}

void PerformanceUi::drawLfoIcons(const UiState& state) {
  drawLfoLabel(state);
  for (std::size_t i = 0; i < lfo_icon_buttons_.size(); ++i) {
    drawLfoIcon(i, state);
  }
}

void PerformanceUi::drawLfoLabel(const UiState& state) const {
  const Rect& rect = lfo_label_area_;
  const std::uint32_t fill = state.lfo_enabled ? SynthConfig::ui.slider_fill_color : SynthConfig::ui.muted_button_color;
  const std::uint32_t border =
      state.lfo_enabled ? SynthConfig::ui.selected_border_color : SynthConfig::ui.muted_border_color;
  const std::uint32_t text =
      state.lfo_enabled ? SynthConfig::ui.selected_text_color : SynthConfig::ui.muted_text_color;
  M5.Display.fillRoundRect(rect.x, rect.y, rect.w, rect.h, SynthConfig::ui.round_radius, fill);
  M5.Display.drawRoundRect(rect.x, rect.y, rect.w, rect.h, SynthConfig::ui.round_radius, border);
  M5.Display.setTextColor(text);
  M5.Display.setTextSize(1);
  M5.Display.setTextDatum(middle_center);
  const int cx = rect.x + rect.w / 2;
  const int y0 = rect.y + rect.h / 2 - 24;
  M5.Display.drawString("L", cx, y0);
  M5.Display.drawString("F", cx, y0 + 24);
  M5.Display.drawString("O", cx, y0 + 48);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(2);
}

void PerformanceUi::drawLfoIcon(std::size_t index, const UiState& state) {
  const UiLfoParameter lfo_param = kUiLfoParameters[index];
  const bool selected = state.lfo_edit_mode && (lfo_param == state.selected_lfo_parameter);
  const float value = lfoValue(state, lfo_param);
  const char* label = lfoLabel(lfo_param);
  lfo_parameter_icons_[index].drawBar(label, value, selected);
}

void PerformanceUi::drawEnvelopePreview(const UiState& state) {
  const auto& rect = envelope_preview_area_;
  if (rect.w <= 0 || rect.h <= 0) {
    return;
  }

  M5.Display.fillRoundRect(rect.x, rect.y, rect.w, rect.h, SynthConfig::ui.round_radius, SynthConfig::ui.muted_button_color);
  M5.Display.drawRoundRect(rect.x, rect.y, rect.w, rect.h, SynthConfig::ui.round_radius, SynthConfig::ui.muted_border_color);
  M5.Display.drawRoundRect(rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2, SynthConfig::ui.round_radius,
                           SynthConfig::ui.muted_border_color);

  const int graph_x = rect.x + 8;
  const int graph_y = rect.y + 10;
  const int graph_w = std::max(16, rect.w - 16);
  const int graph_h = std::max(16, rect.h - 20);
  const int bottom = graph_y + graph_h - 1;
  const int top = graph_y + 2;

  const float attack = std::clamp(state.attack, 0.0f, 1.0f);
  const float decay = std::clamp(state.decay, 0.0f, 1.0f);
  const float sustain = std::clamp(state.sustain, 0.0f, 1.0f);
  const float release = std::clamp(state.release, 0.0f, 1.0f);
  constexpr float kHold = 0.35f;
  constexpr float kMinSegment = 0.05f;
  const float total = std::max(0.0001f, attack + decay + release + kHold);

  const int x0 = graph_x + 1;
  const int x4 = graph_x + graph_w - 2;
  const auto segment_to_x = [&](float start, float span) {
    const float normalized = std::clamp((start + span) / total, 0.0f, 1.0f);
    return x0 + static_cast<int>(std::round((x4 - x0) * normalized));
  };

  const float a_span = std::max(kMinSegment, attack);
  const float d_span = std::max(kMinSegment, decay);
  const float s_span = std::max(kMinSegment, kHold);

  const int x1 = segment_to_x(0.0f, a_span);
  const int x2 = segment_to_x(a_span, d_span);
  const int x3 = segment_to_x(a_span + d_span, s_span);

  const int y0 = bottom;
  const int y1 = top;
  const int ys = bottom - static_cast<int>(std::round((bottom - top) * sustain));

  const std::uint32_t env_color = SynthConfig::ui.slider_fill_color;
  M5.Display.drawLine(x0, y0, x1, y1, env_color);
  M5.Display.drawLine(x1, y1, x2, ys, env_color);
  M5.Display.drawLine(x2, ys, x3, ys, env_color);
  M5.Display.drawLine(x3, ys, x4, y0, env_color);
}

void PerformanceUi::drawWaveformIcon(const Rect& rect, Waveform waveform, std::uint32_t color) {
  const int left = rect.x + kWaveIconInsetX;
  const int right = rect.x + rect.w - kWaveIconInsetX;
  const int top = rect.y + kWaveIconInsetTop;
  const int bottom = rect.y + rect.h - kWaveIconInsetBottom;
  const int mid_y = (top + bottom) / 2;
  const int high_y = top + 4;
  const int low_y = bottom - 4;

  switch (waveform) {
    case Waveform::Sine: {
      const int steps = 24;
      for (int t = 0; t < SynthConfig::ui.wave_icon_thickness; ++t) {
        int prev_x = left;
        int prev_y = mid_y + t;
        for (int i = 1; i <= steps; ++i) {
          const float phase = static_cast<float>(i) / static_cast<float>(steps);
          const int x = left + ((right - left) * i) / steps;
          const int y = mid_y - static_cast<int>(std::round(std::sinf(phase * 6.28318530718f) * ((bottom - top) * 0.35f))) + t;
          M5.Display.drawLine(prev_x, prev_y, x, y, color);
          prev_x = x;
          prev_y = y;
        }
      }
      break;
    }
    case Waveform::Saw:
      for (int t = 0; t < SynthConfig::ui.wave_icon_thickness; ++t) {
        M5.Display.drawLine(left, low_y + t, right - 10, high_y + t, color);
        M5.Display.drawLine(right - 10, high_y + t, right - 10, low_y + t, color);
      }
      break;
    case Waveform::Square:
      for (int t = 0; t < SynthConfig::ui.wave_icon_thickness; ++t) {
        M5.Display.drawLine(left, low_y + t, left + 12, low_y + t, color);
        M5.Display.drawLine(left + 12, low_y + t, left + 12, high_y + t, color);
        M5.Display.drawLine(left + 12, high_y + t, right - 12, high_y + t, color);
        M5.Display.drawLine(right - 12, high_y + t, right - 12, low_y + t, color);
        M5.Display.drawLine(right - 12, low_y + t, right, low_y + t, color);
      }
      break;
    case Waveform::Triangle:
      for (int t = 0; t < SynthConfig::ui.wave_icon_thickness; ++t) {
        M5.Display.drawLine(left, low_y + t, (left + right) / 2, high_y + t, color);
        M5.Display.drawLine((left + right) / 2, high_y + t, right, low_y + t, color);
      }
      break;
    default:
      break;
  }
}

void PerformanceUi::drawSourceLabel(const Rect& rect, AudioSourceType source, std::uint32_t color) {
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(color);
  M5.Display.setTextSize(SynthConfig::ui.selection_button_text_size);
  M5.Display.drawString(audioSourceLabel(source), rect.x + rect.w / 2, rect.y + rect.h / 2);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(2);
}

void PerformanceUi::drawSliderControl(const UiState& state) {
  M5.Display.fillRoundRect(slider_area_.x, slider_area_.y, slider_area_.w, slider_area_.h,
                           SynthConfig::ui.round_radius, SynthConfig::ui.muted_button_color);
  M5.Display.drawRoundRect(slider_area_.x, slider_area_.y, slider_area_.w, slider_area_.h,
                           SynthConfig::ui.round_radius, SynthConfig::ui.muted_border_color);

  float value = parameterValue(state, state.selected_parameter);
  if (state.lfo_edit_mode) {
    value = lfoValue(state, state.selected_lfo_parameter);
  }
  const int track_x = slider_area_.x + SynthConfig::ui.slider_inset;
  const int track_y = slider_area_.y + (slider_area_.h - SynthConfig::ui.slider_track_height) / 2;
  const int track_w = slider_area_.w - (SynthConfig::ui.slider_inset * 2);
  const int fill_w = std::max(8, static_cast<int>(std::round(track_w * value)));

  M5.Display.fillRoundRect(track_x, track_y, track_w, SynthConfig::ui.slider_track_height, 9,
                           SynthConfig::ui.slider_track_color);
  M5.Display.drawRoundRect(track_x, track_y, track_w, SynthConfig::ui.slider_track_height, 9,
                           SynthConfig::ui.muted_border_color);
  M5.Display.fillRoundRect(track_x, track_y, std::min(fill_w, track_w), SynthConfig::ui.slider_track_height, 9,
                           SynthConfig::ui.slider_fill_color);

  const int knob_x = track_x + static_cast<int>(std::round((track_w - 1) * value));
  M5.Display.fillCircle(knob_x, track_y + SynthConfig::ui.slider_track_height / 2, 14,
                        SynthConfig::ui.selected_border_color);
  M5.Display.drawCircle(knob_x, track_y + SynthConfig::ui.slider_track_height / 2, 14,
                        SynthConfig::ui.panel_color);
}

void PerformanceUi::drawMemorySlots(const UiState& state) {
  for (std::size_t i = 0; i < memory_slot_buttons_.size(); ++i) {
    drawMemorySlot(i, state);
  }
}

void PerformanceUi::drawPresets(const UiState& state) {
  for (std::size_t i = 0; i < preset_buttons_.size(); ++i) {
    drawPreset(i, state);
  }
}

void PerformanceUi::drawPreset(std::size_t index, const UiState& state) {
  const Rect& rect = preset_buttons_[index];
  const bool selected = state.preset_active && state.selected_preset == index;
  const std::uint32_t fill = selected ? SynthConfig::ui.slider_fill_color : SynthConfig::ui.panel_color;
  const std::uint32_t border = selected ? SynthConfig::ui.selected_border_color : SynthConfig::ui.muted_border_color;
  const std::uint32_t text = selected ? SynthConfig::ui.selected_text_color : SynthConfig::ui.muted_text_color;

  M5.Display.fillRoundRect(rect.x, rect.y, rect.w, rect.h, SynthConfig::ui.round_radius, fill);
  M5.Display.drawRoundRect(rect.x, rect.y, rect.w, rect.h, SynthConfig::ui.round_radius, border);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(text);
  M5.Display.setTextSize(1);
  M5.Display.drawString(kPresetLabels[index], rect.x + rect.w / 2, rect.y + rect.h / 2);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(2);
}

void PerformanceUi::drawMemorySlot(std::size_t index, const UiState& state) {
  const Rect& rect = memory_slot_buttons_[index];
  const bool selected = (!state.preset_active) && state.selected_memory_slot == index;
  const std::uint32_t fill = selected ? SynthConfig::ui.slider_fill_color : SynthConfig::ui.muted_button_color;
  const std::uint32_t border = selected ? SynthConfig::ui.selected_border_color : SynthConfig::ui.muted_border_color;
  const std::uint32_t text = selected ? SynthConfig::ui.selected_text_color : SynthConfig::ui.muted_text_color;
  char label[3] = {'M', static_cast<char>('1' + static_cast<int>(index)), '\0'};

  M5.Display.fillRoundRect(rect.x, rect.y, rect.w, rect.h, SynthConfig::ui.round_radius, fill);
  M5.Display.drawRoundRect(rect.x, rect.y, rect.w, rect.h, SynthConfig::ui.round_radius, border);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(text);
  M5.Display.setTextSize(1);
  M5.Display.drawString(label, rect.x + rect.w / 2, rect.y + rect.h / 2);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(2);
}

void PerformanceUi::drawPitchModeSwitch(const UiState& state) {
  M5.Display.fillRoundRect(pitch_mode_area_.x, pitch_mode_area_.y, pitch_mode_area_.w, pitch_mode_area_.h,
                           SynthConfig::ui.round_radius, SynthConfig::ui.muted_button_color);
  M5.Display.drawRoundRect(pitch_mode_area_.x, pitch_mode_area_.y, pitch_mode_area_.w, pitch_mode_area_.h,
                           SynthConfig::ui.round_radius, SynthConfig::ui.muted_border_color);
  drawPitchModeButton(semitone_button_, "SEMI", state.quantize_to_semitone);
  drawPitchModeButton(continuous_button_, "CONT", !state.quantize_to_semitone);
}

void PerformanceUi::drawPitchModeButton(const Rect& rect, const char* label, bool selected) {
  const std::uint32_t fill = selected ? SynthConfig::ui.slider_fill_color : SynthConfig::ui.panel_color;
  const std::uint32_t border = selected ? SynthConfig::ui.selected_border_color : SynthConfig::ui.muted_border_color;
  const std::uint32_t text = selected ? SynthConfig::ui.selected_text_color : SynthConfig::ui.muted_text_color;
  M5.Display.fillRoundRect(rect.x, rect.y, rect.w, rect.h, SynthConfig::ui.round_radius, fill);
  M5.Display.drawRoundRect(rect.x, rect.y, rect.w, rect.h, SynthConfig::ui.round_radius, border);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(text);
  M5.Display.setTextSize(1);
  M5.Display.drawString(label, rect.x + rect.w / 2, rect.y + rect.h / 2);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(2);
}

void PerformanceUi::drawPerformanceBase() {
  M5.Display.fillRect(performance_area_.x - 2, performance_area_.y - 2, performance_area_.w + 4, performance_area_.h + 4,
                      SynthConfig::ui.background_color);
  M5.Display.fillRect(performance_area_.x, performance_area_.y, performance_area_.w, performance_area_.h,
                      SynthConfig::ui.panel_color);
  M5.Display.drawRect(performance_area_.x, performance_area_.y, performance_area_.w, performance_area_.h,
                      SynthConfig::ui.panel_border_color);
  drawCenterGuides(performance_area_);
}

void PerformanceUi::drawKeyboardBase() {
  M5.Display.fillRect(keyboard_area_.x, keyboard_area_.y, keyboard_area_.w, keyboard_area_.h,
                      SynthConfig::ui.keyboard_white_color);
  M5.Display.drawRect(keyboard_area_.x, keyboard_area_.y, keyboard_area_.w, keyboard_area_.h,
                      SynthConfig::ui.keyboard_line_color);

  const int octaves = std::max(1, SynthConfig::ui.keyboard_octaves);
  const int total_white = octaves * 7 + 1;
  const float white_w = static_cast<float>(keyboard_area_.w) / static_cast<float>(total_white);

  for (int white_index = 0; white_index < total_white; ++white_index) {
    const int x = keyboard_area_.x + static_cast<int>(std::round(white_index * white_w));
    const int next_x = keyboard_area_.x + static_cast<int>(std::round((white_index + 1) * white_w));
    M5.Display.fillRect(x, keyboard_area_.y, next_x - x, keyboard_area_.h, SynthConfig::ui.keyboard_white_color);
    M5.Display.drawRect(x, keyboard_area_.y, next_x - x, keyboard_area_.h, SynthConfig::ui.keyboard_line_color);
  }

  UiState empty_state{};
  redrawBlackKeys(empty_state);
}

void PerformanceUi::drawKeyboardOverlays(const UiState& state) {
  for (std::size_t i = 0; i < state.keyboard_note_count; ++i) {
    drawKeyboardNote(state.keyboard_notes[i], true);
  }
}

void PerformanceUi::drawBlackKey(int octave, std::size_t black_index, bool active) {
  const int total_white = std::max(1, SynthConfig::ui.keyboard_octaves) * 7 + 1;
  const float white_w = static_cast<float>(keyboard_area_.w) / static_cast<float>(total_white);
  const float black_w = white_w * SynthConfig::ui.keyboard_black_width_ratio;
  const int black_h = static_cast<int>(std::round(keyboard_area_.h * SynthConfig::ui.keyboard_black_height_ratio));
  const float center = keyboard_area_.x +
                       (static_cast<float>(octave * 7 + kBlackWhiteSlot[black_index] + 1) * white_w);
  const int left = static_cast<int>(std::round(center - (black_w * 0.5f)));
  const int width = static_cast<int>(std::round(black_w));

  M5.Display.fillRect(left, keyboard_area_.y, width, black_h,
                      active ? SynthConfig::ui.selected_border_color : SynthConfig::ui.keyboard_black_color);
  M5.Display.drawRect(left, keyboard_area_.y, width, black_h, SynthConfig::ui.keyboard_line_color);
}

void PerformanceUi::redrawBlackKeys(const UiState& state) {
  const int octaves = std::max(1, SynthConfig::ui.keyboard_octaves);
  for (int octave = 0; octave < octaves; ++octave) {
    for (std::size_t i = 0; i < kBlackSemitones.size(); ++i) {
      const int note = SynthConfig::ui.keyboard_root_note + octave * 12 + kBlackSemitones[i];
      drawBlackKey(octave, i, hasKeyboardNote(state, note));
    }
  }
}

void PerformanceUi::drawKeyboardNote(int note, bool active) {
  const int octaves = std::max(1, SynthConfig::ui.keyboard_octaves);
  const int total_white = octaves * 7 + 1;
  const float white_w = static_cast<float>(keyboard_area_.w) / static_cast<float>(total_white);
  const int relative = note - SynthConfig::ui.keyboard_root_note;

  if (relative < 0 || relative > octaves * 12) {
    return;
  }

  if (relative == octaves * 12) {
    const int white_index = total_white - 1;
    const int x = keyboard_area_.x + static_cast<int>(std::round(white_index * white_w));
    const int next_x = keyboard_area_.x + static_cast<int>(std::round((white_index + 1) * white_w));
    M5.Display.fillRect(x, keyboard_area_.y, next_x - x, keyboard_area_.h,
                        active ? SynthConfig::ui.slider_fill_color : SynthConfig::ui.keyboard_white_color);
    M5.Display.drawRect(x, keyboard_area_.y, next_x - x, keyboard_area_.h, SynthConfig::ui.keyboard_line_color);
    return;
  }

  const int octave = relative / 12;
  const int semitone = relative % 12;

  if (isBlackKeySemitone(semitone)) {
    std::size_t black_index = 0;
    for (; black_index < kBlackSemitones.size(); ++black_index) {
      if (kBlackSemitones[black_index] == semitone) {
        break;
      }
    }
    if (black_index >= kBlackSemitones.size()) {
      return;
    }
    drawBlackKey(octave, black_index, active);
    return;
  }

  std::size_t white_step = 0;
  for (; white_step < kWhiteSemitones.size(); ++white_step) {
    if (kWhiteSemitones[white_step] == semitone) {
      break;
    }
  }
  if (white_step >= kWhiteSemitones.size()) {
    return;
  }
  const int white_index = octave * 7 + static_cast<int>(white_step);
  const int x = keyboard_area_.x + static_cast<int>(std::round(white_index * white_w));
  const int next_x = keyboard_area_.x + static_cast<int>(std::round((white_index + 1) * white_w));
  M5.Display.fillRect(x, keyboard_area_.y, next_x - x, keyboard_area_.h,
                      active ? SynthConfig::ui.slider_fill_color : SynthConfig::ui.keyboard_white_color);
  M5.Display.drawRect(x, keyboard_area_.y, next_x - x, keyboard_area_.h, SynthConfig::ui.keyboard_line_color);
}

void PerformanceUi::eraseTouchMarkers() {
  for (std::size_t i = 0; i < previous_touch_count_; ++i) {
    eraseMarkerAt(previous_touch_xs_[i], previous_touch_ys_[i]);
  }
  M5.Display.drawRect(performance_area_.x, performance_area_.y, performance_area_.w, performance_area_.h,
                      SynthConfig::ui.panel_border_color);
  drawCenterGuides(performance_area_);
  previous_touch_count_ = 0;
}

void PerformanceUi::drawTouchMarkers(const UiState& state) {
  if (state.touch_count == 0) {
    previous_touch_xs_.fill(0);
    previous_touch_ys_.fill(0);
    previous_touch_count_ = 0;
    return;
  }

  const std::uint32_t fill = state.selected_source == AudioSourceType::Oscillator
                                 ? waveformColor(state.selected_waveform)
                                 : SynthConfig::ui.slider_fill_color;

  for (std::size_t i = 0; i < state.touch_count; ++i) {
    const int draw_x = std::clamp(state.touch_xs[i], performance_area_.x + SynthConfig::ui.marker_radius,
                                  performance_area_.x + performance_area_.w - SynthConfig::ui.marker_radius);
    const int draw_y = std::clamp(state.touch_ys[i], performance_area_.y + SynthConfig::ui.marker_radius,
                                  performance_area_.y + performance_area_.h - SynthConfig::ui.marker_radius);
    drawMarkerAt(draw_x, draw_y, fill);
    previous_touch_xs_[i] = draw_x;
    previous_touch_ys_[i] = draw_y;
  }
  previous_touch_count_ = state.touch_count;
}

void PerformanceUi::drawCenterGuides(const Rect& bounds) {
  const int center_x = performance_area_.x + performance_area_.w / 2;
  const int center_y = performance_area_.y + performance_area_.h / 2;
  const int left = std::max(bounds.x, performance_area_.x + 1);
  const int right = std::min(bounds.x + bounds.w - 1, performance_area_.x + performance_area_.w - 2);
  const int top = std::max(bounds.y, performance_area_.y + 1);
  const int bottom = std::min(bounds.y + bounds.h - 1, performance_area_.y + performance_area_.h - 2);
  const int step = SynthConfig::ui.center_guide_segment + SynthConfig::ui.center_guide_gap;

  if (center_y >= top && center_y <= bottom) {
    for (int x = left; x <= right; x += step) {
      const int x2 = std::min(x + SynthConfig::ui.center_guide_segment, right);
      M5.Display.drawLine(x, center_y, x2, center_y, SynthConfig::ui.panel_border_color);
    }
  }

  if (center_x >= left && center_x <= right) {
    for (int y = top; y <= bottom; y += step) {
      const int y2 = std::min(y + SynthConfig::ui.center_guide_segment, bottom);
      M5.Display.drawLine(center_x, y, center_x, y2, SynthConfig::ui.panel_border_color);
    }
  }
}

void PerformanceUi::eraseMarkerAt(int x, int y) {
  const int draw_x = std::clamp(x, performance_area_.x + SynthConfig::ui.marker_radius,
                                performance_area_.x + performance_area_.w - SynthConfig::ui.marker_radius);
  const int draw_y = std::clamp(y, performance_area_.y + SynthConfig::ui.marker_radius,
                                performance_area_.y + performance_area_.h - SynthConfig::ui.marker_radius);
  M5.Display.fillCircle(draw_x, draw_y, SynthConfig::ui.marker_radius + 1, SynthConfig::ui.panel_color);
}

void PerformanceUi::drawMarkerAt(int x, int y, std::uint32_t fill) {
  const int draw_x = std::clamp(x, performance_area_.x + SynthConfig::ui.marker_radius,
                                performance_area_.x + performance_area_.w - SynthConfig::ui.marker_radius);
  const int draw_y = std::clamp(y, performance_area_.y + SynthConfig::ui.marker_radius,
                                performance_area_.y + performance_area_.h - SynthConfig::ui.marker_radius);
  M5.Display.fillCircle(draw_x, draw_y, SynthConfig::ui.marker_radius, fill);
  M5.Display.drawCircle(draw_x, draw_y, SynthConfig::ui.marker_radius, SynthConfig::ui.selected_border_color);
}
