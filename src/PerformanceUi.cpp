#include "PerformanceUi.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace {

constexpr int kWaveLabelBottomPadding = 10;
constexpr int kWaveIconInsetX = 10;
constexpr int kWaveIconInsetTop = 18;
constexpr int kWaveIconInsetBottom = 18;

int waveRowFromIndex(int index) {
  return index / SynthConfig::ui.wave_button_columns;
}

int waveColFromIndex(int index) {
  return index % SynthConfig::ui.wave_button_columns;
}

}  // namespace

void PerformanceUi::begin() {
  layout();
}

void PerformanceUi::drawInitial(const UiState& state) {
  M5.Display.fillScreen(SynthConfig::ui.background_color);
  drawWaveformButtons(state);
  drawVolumeControl(state);
  drawPerformanceBase();
  drawCoordinateText(state);
  drawTouchMarker(state);
}

void PerformanceUi::refreshPerformance(const UiState& state) {
  drawCoordinateText(state);
  drawTouchMarker(state);
}

void PerformanceUi::refreshWaveformSelection(Waveform previous, const UiState& state) {
  drawWaveformButton(previous, state);
  drawWaveformButton(state.selected_waveform, state);
  drawCoordinateText(state);
}

void PerformanceUi::refreshVolumeControl(const UiState& state) {
  drawVolumeControl(state);
}

bool PerformanceUi::isWaveformArea(int x) const {
  return x < performance_area_.x;
}

bool PerformanceUi::isPerformanceArea(int x, int y) const {
  return performance_area_.contains(x, y);
}

bool PerformanceUi::isVolumeArea(int x, int y) const {
  return volume_area_.contains(x, y);
}

Waveform PerformanceUi::waveformAt(int x, int y, Waveform fallback) const {
  for (int i = 0; i < static_cast<int>(Waveform::Count); ++i) {
    const auto waveform = static_cast<Waveform>(i);
    if (waveform_buttons_[static_cast<std::size_t>(i)].contains(x, y)) {
      return waveform;
    }
  }
  return fallback;
}

int PerformanceUi::xToMidiNote(int x) const {
  const int clamped_x = std::clamp(x, performance_area_.x, performance_area_.x + performance_area_.w - 1);
  const float normalized = static_cast<float>(clamped_x - performance_area_.x) /
                           static_cast<float>(std::max(1, performance_area_.w - 1));
  const int note_span = SynthConfig::ui.max_midi_note - SynthConfig::ui.min_midi_note;
  return SynthConfig::ui.min_midi_note + static_cast<int>(std::round(normalized * note_span));
}

float PerformanceUi::volumeFromTouch(int x) const {
  const int slider_left = volume_area_.x + SynthConfig::ui.slider_inset;
  const int slider_right = volume_area_.x + volume_area_.w - SynthConfig::ui.slider_inset -
                           SynthConfig::ui.volume_value_width - SynthConfig::ui.volume_value_gap;
  const int clamped_x = std::clamp(x, slider_left, slider_right);
  return static_cast<float>(clamped_x - slider_left) / static_cast<float>(std::max(1, slider_right - slider_left));
}

int PerformanceUi::normalizedTouchX(int x) const {
  const int center_x = performance_area_.x + performance_area_.w / 2;
  const float half_width = std::max(1.0f, static_cast<float>(performance_area_.w) / 2.0f);
  const float normalized = (static_cast<float>(x - center_x) / half_width) * 100.0f;
  return std::clamp(static_cast<int>(std::round(normalized)), -100, 100);
}

int PerformanceUi::normalizedTouchY(int y) const {
  const int center_y = performance_area_.y + performance_area_.h / 2;
  const float half_height = std::max(1.0f, static_cast<float>(performance_area_.h) / 2.0f);
  const float normalized = (static_cast<float>(center_y - y) / half_height) * 100.0f;
  return std::clamp(static_cast<int>(std::round(normalized)), -100, 100);
}

void PerformanceUi::layout() {
  const int width = M5.Display.width();
  const int height = M5.Display.height();
  const int performance_width = std::clamp(
      static_cast<int>(std::round(width * SynthConfig::ui.performance_area_width_ratio)), width / 4, width - 160);
  const int left_width = width - performance_width;

  performance_area_ = {left_width, 0, performance_width, height};
  coordinate_text_area_ = {
      performance_area_.x + SynthConfig::ui.coordinate_text_margin_left,
      performance_area_.y + performance_area_.h - SynthConfig::ui.coordinate_text_margin_bottom -
          SynthConfig::ui.coordinate_text_height,
      SynthConfig::ui.coordinate_text_width,
      SynthConfig::ui.coordinate_text_height};

  const int columns = std::max(1, SynthConfig::ui.wave_button_columns);
  const int base_button_width =
      (left_width - (SynthConfig::ui.wave_button_padding * 2) - (SynthConfig::ui.wave_button_gap * (columns - 1))) /
      columns;
  const int button_width =
      std::max(48, static_cast<int>(std::round(base_button_width * SynthConfig::ui.wave_button_width_scale)));

  for (int i = 0; i < static_cast<int>(Waveform::Count); ++i) {
    const int col = waveColFromIndex(i);
    const int row = waveRowFromIndex(i);
    const int x = SynthConfig::ui.wave_button_padding + col * (base_button_width + SynthConfig::ui.wave_button_gap);
    const int y = SynthConfig::ui.wave_button_padding +
                  row * (SynthConfig::ui.wave_button_height + SynthConfig::ui.wave_button_gap);
    waveform_buttons_[static_cast<std::size_t>(i)] = {x, y, button_width, SynthConfig::ui.wave_button_height};
  }

  int bottom = 0;
  for (const auto& rect : waveform_buttons_) {
    bottom = std::max(bottom, rect.y + rect.h);
  }

  const int volume_y = bottom + SynthConfig::ui.wave_button_gap;
  volume_area_ = {SynthConfig::ui.wave_button_padding, volume_y,
                  left_width - (SynthConfig::ui.wave_button_padding * 2), SynthConfig::ui.volume_area_height};
}

void PerformanceUi::drawWaveformButtons(const UiState& state) {
  for (int i = 0; i < static_cast<int>(Waveform::Count); ++i) {
    drawWaveformButton(static_cast<Waveform>(i), state);
  }
}

void PerformanceUi::drawWaveformButton(Waveform waveform, const UiState& state) {
  const auto& rect = waveform_buttons_[static_cast<std::size_t>(waveform)];
  const bool selected = waveform == state.selected_waveform;
  const std::uint32_t fill = selected ? waveformColor(waveform) : SynthConfig::ui.muted_button_color;
  const std::uint32_t border = selected ? SynthConfig::ui.selected_border_color : SynthConfig::ui.muted_border_color;
  const std::uint32_t fg = selected ? SynthConfig::ui.selected_text_color : SynthConfig::ui.muted_text_color;

  M5.Display.fillRoundRect(rect.x, rect.y, rect.w, rect.h, SynthConfig::ui.round_radius, fill);
  M5.Display.drawRoundRect(rect.x, rect.y, rect.w, rect.h, SynthConfig::ui.round_radius, border);
  M5.Display.drawRoundRect(rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2, SynthConfig::ui.round_radius, border);
  drawWaveformIcon(rect, waveform, fg);

  if (SynthConfig::ui.show_wave_labels) {
    M5.Display.setTextColor(fg);
    M5.Display.setTextDatum(bottom_center);
    M5.Display.setTextSize(SynthConfig::ui.wave_label_text_size);
    M5.Display.drawString(waveformLabel(waveform), rect.x + rect.w / 2, rect.y + rect.h - kWaveLabelBottomPadding);
    M5.Display.setTextSize(2);
  }
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

void PerformanceUi::drawVolumeControl(const UiState& state) {
  M5.Display.fillRoundRect(volume_area_.x, volume_area_.y, volume_area_.w, volume_area_.h,
                           SynthConfig::ui.round_radius, SynthConfig::ui.muted_button_color);
  M5.Display.drawRoundRect(volume_area_.x, volume_area_.y, volume_area_.w, volume_area_.h,
                           SynthConfig::ui.round_radius, SynthConfig::ui.muted_border_color);

  const int track_x = volume_area_.x + SynthConfig::ui.slider_inset;
  const int track_y = volume_area_.y + (volume_area_.h - SynthConfig::ui.slider_track_height) / 2;
  const int track_w = volume_area_.w - (SynthConfig::ui.slider_inset * 2) - SynthConfig::ui.volume_value_width -
                      SynthConfig::ui.volume_value_gap;
  const int fill_w = std::max(8, static_cast<int>(std::round(track_w * state.volume)));
  const int value_x = track_x + track_w + SynthConfig::ui.volume_value_gap;

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

  char volume_text[8];
  std::snprintf(volume_text, sizeof(volume_text), "%3d", static_cast<int>(std::round(state.volume * 100.0f)));
  M5.Display.setTextColor(SynthConfig::ui.muted_text_color);
  M5.Display.setTextDatum(middle_right);
  M5.Display.drawString(volume_text, value_x + SynthConfig::ui.volume_value_width,
                        track_y + SynthConfig::ui.slider_track_height / 2);
  M5.Display.setTextDatum(top_left);
}

void PerformanceUi::drawPerformanceBase() {
  M5.Display.fillRect(performance_area_.x, performance_area_.y, performance_area_.w, performance_area_.h,
                      SynthConfig::ui.panel_color);
  M5.Display.drawRect(performance_area_.x, performance_area_.y, performance_area_.w, performance_area_.h,
                      SynthConfig::ui.panel_border_color);
  drawCenterGuides(performance_area_);
}

void PerformanceUi::drawCoordinateText(const UiState& state) {
  M5.Display.fillRect(coordinate_text_area_.x, coordinate_text_area_.y, coordinate_text_area_.w, coordinate_text_area_.h,
                      SynthConfig::ui.panel_color);

  const bool has_touch = state.note_playing && state.last_touch_x >= 0 && state.last_touch_y >= 0;
  const int normalized_x = has_touch ? normalizedTouchX(state.last_touch_x) : 0;
  const int normalized_y = has_touch ? normalizedTouchY(state.last_touch_y) : 0;

  char text[32];
  std::snprintf(text, sizeof(text), "x = %+04d, y = %+04d", normalized_x, normalized_y);

  M5.Display.setTextDatum(top_left);
  M5.Display.setTextColor(SynthConfig::ui.muted_text_color);
  M5.Display.setTextSize(SynthConfig::ui.coordinate_text_size);
  M5.Display.drawString(text, coordinate_text_area_.x, coordinate_text_area_.y + 2);
  M5.Display.setTextSize(2);
}

void PerformanceUi::drawTouchMarker(const UiState& state) {
  const Rect marker_zone = {
      performance_area_.x + SynthConfig::ui.marker_side_margin,
      performance_area_.y + SynthConfig::ui.marker_min_top,
      performance_area_.w - (SynthConfig::ui.marker_side_margin * 2),
      performance_area_.h - SynthConfig::ui.marker_min_top - SynthConfig::ui.marker_bottom_margin};

  M5.Display.fillRect(marker_zone.x, marker_zone.y, marker_zone.w, marker_zone.h, SynthConfig::ui.panel_color);
  drawCenterGuides(marker_zone);

  if (!state.note_playing || state.last_touch_x < 0 || state.last_touch_y < 0) {
    return;
  }

  const int draw_x = std::clamp(state.last_touch_x, marker_zone.x + SynthConfig::ui.marker_radius,
                                marker_zone.x + marker_zone.w - SynthConfig::ui.marker_radius);
  const int draw_y = std::clamp(state.last_touch_y, marker_zone.y + SynthConfig::ui.marker_radius,
                                marker_zone.y + marker_zone.h - SynthConfig::ui.marker_radius);
  M5.Display.fillCircle(draw_x, draw_y, SynthConfig::ui.marker_radius, waveformColor(state.selected_waveform));
  M5.Display.drawCircle(draw_x, draw_y, SynthConfig::ui.marker_radius, SynthConfig::ui.selected_border_color);
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
