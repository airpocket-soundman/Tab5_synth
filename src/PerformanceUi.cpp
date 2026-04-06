#include "PerformanceUi.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace {

constexpr int kWaveIconInsetX = 10;
constexpr int kWaveIconInsetTop = 18;
constexpr int kWaveIconInsetBottom = 18;

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
  drawParameterIcon(state);
  drawPerformanceBase();
  drawPitchModeSwitch(state);
  drawVolumeControl(state);
  previous_touch_count_ = 0;
  previous_touch_xs_.fill(0);
  previous_touch_ys_.fill(0);
  drawTouchMarkers(state);
}

void PerformanceUi::refreshPerformance(const UiState& state) {
  eraseTouchMarkers();
  drawTouchMarkers(state);
}

void PerformanceUi::refreshSourceSelection(const UiState& state) {
  drawSourceButtons(state);
}

void PerformanceUi::refreshParameterSelection(const UiState& state) {
  drawParameterIcon(state);
}

void PerformanceUi::refreshVolumeControl(const UiState& state) {
  drawParameterIcon(state);
  drawVolumeControl(state);
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
  return parameter_icon_.contains(x, y);
}

bool PerformanceUi::isPerformanceArea(int x, int y) const {
  return performance_area_.contains(x, y);
}

bool PerformanceUi::isVolumeArea(int x, int y) const {
  return volume_area_.contains(x, y);
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
  if (parameter_button_.contains(x, y)) {
    return UiParameter::Volume;
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

float PerformanceUi::volumeFromTouch(int x) const {
  const int slider_left = volume_area_.x + SynthConfig::ui.slider_inset;
  const int slider_right = volume_area_.x + volume_area_.w - SynthConfig::ui.slider_inset;
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

void PerformanceUi::layout() {
  const int width = M5.Display.width();
  const int height = M5.Display.height();
  const int right_zone_width = std::clamp(
      static_cast<int>(std::round(width * SynthConfig::ui.performance_area_width_ratio)), width / 4, width - 160);
  const int left_width = width - right_zone_width;

  const int total_buttons = static_cast<int>(source_buttons_.size()) + 1;
  const int button_width =
      (left_width - (SynthConfig::ui.wave_button_padding * 2) -
       (SynthConfig::ui.selection_button_gap * (total_buttons - 1))) /
      total_buttons;

  for (std::size_t i = 0; i < source_buttons_.size(); ++i) {
    const int x = SynthConfig::ui.wave_button_padding +
                  static_cast<int>(i) * (button_width + SynthConfig::ui.selection_button_gap);
    const int y = SynthConfig::ui.wave_button_padding;
    source_buttons_[i] = {x, y, button_width, SynthConfig::ui.selection_button_height};
  }

  parameter_button_ = {source_buttons_.back().x + source_buttons_.back().w + SynthConfig::ui.selection_button_gap,
                       SynthConfig::ui.wave_button_padding, button_width, SynthConfig::ui.selection_button_height};
  parameter_icon_.begin(parameter_button_);

  const int right_zone_x = left_width;
  const int square_limit = std::min(right_zone_width - (SynthConfig::ui.wave_button_padding * 2),
                                    height - (SynthConfig::ui.wave_button_padding * 2) -
                                        (SynthConfig::ui.pitch_mode_height * 2) -
                                        SynthConfig::ui.pitch_mode_gap * 2);
  const int square_size = std::max(180, static_cast<int>(std::round(square_limit * SynthConfig::ui.performance_square_scale)));
  const int square_x = right_zone_x + right_zone_width - square_size - SynthConfig::ui.wave_button_padding;
  const int square_y = SynthConfig::ui.performance_square_top_margin;
  performance_area_ = {square_x, square_y, square_size, square_size};

  const int pitch_y = performance_area_.y + performance_area_.h + SynthConfig::ui.pitch_mode_gap;
  pitch_mode_area_ = {performance_area_.x, pitch_y, performance_area_.w, SynthConfig::ui.pitch_mode_height};

  const int mode_button_width = (pitch_mode_area_.w - SynthConfig::ui.pitch_mode_button_gap) / 2;
  semitone_button_ = {pitch_mode_area_.x, pitch_mode_area_.y, mode_button_width, pitch_mode_area_.h};
  continuous_button_ = {pitch_mode_area_.x + mode_button_width + SynthConfig::ui.pitch_mode_button_gap,
                        pitch_mode_area_.y, mode_button_width, pitch_mode_area_.h};

  const int volume_y = pitch_mode_area_.y + pitch_mode_area_.h + SynthConfig::ui.pitch_mode_gap;
  volume_area_ = {pitch_mode_area_.x, volume_y, pitch_mode_area_.w, SynthConfig::ui.pitch_mode_height};
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

void PerformanceUi::drawParameterIcon(const UiState& state) {
  parameter_icon_.drawVolume(state.volume, state.selected_parameter == UiParameter::Volume);
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

void PerformanceUi::drawVolumeControl(const UiState& state) {
  M5.Display.fillRoundRect(volume_area_.x, volume_area_.y, volume_area_.w, volume_area_.h,
                           SynthConfig::ui.round_radius, SynthConfig::ui.muted_button_color);
  M5.Display.drawRoundRect(volume_area_.x, volume_area_.y, volume_area_.w, volume_area_.h,
                           SynthConfig::ui.round_radius, SynthConfig::ui.muted_border_color);

  const int track_x = volume_area_.x + SynthConfig::ui.slider_inset;
  const int track_y = volume_area_.y + (volume_area_.h - SynthConfig::ui.slider_track_height) / 2;
  const int track_w = volume_area_.w - (SynthConfig::ui.slider_inset * 2);
  const int fill_w = std::max(8, static_cast<int>(std::round(track_w * state.volume)));

  M5.Display.fillRoundRect(track_x, track_y, track_w, SynthConfig::ui.slider_track_height, 9,
                           SynthConfig::ui.slider_track_color);
  M5.Display.drawRoundRect(track_x, track_y, track_w, SynthConfig::ui.slider_track_height, 9,
                           SynthConfig::ui.muted_border_color);
  M5.Display.fillRoundRect(track_x, track_y, std::min(fill_w, track_w), SynthConfig::ui.slider_track_height, 9,
                           SynthConfig::ui.slider_fill_color);

  const int knob_x = track_x + static_cast<int>(std::round((track_w - 1) * state.volume));
  M5.Display.fillCircle(knob_x, track_y + SynthConfig::ui.slider_track_height / 2, 14,
                        SynthConfig::ui.selected_border_color);
  M5.Display.drawCircle(knob_x, track_y + SynthConfig::ui.slider_track_height / 2, 14,
                        SynthConfig::ui.panel_color);
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

void PerformanceUi::eraseTouchMarkers() {
  for (std::size_t i = 0; i < previous_touch_count_; ++i) {
    eraseMarkerAt(previous_touch_xs_[i], previous_touch_ys_[i]);
  }
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
