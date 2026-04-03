#include "PerformanceUi.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

constexpr int kWaveButtonGap = 12;
constexpr int kWaveButtonHeight = 120;
constexpr int kWaveButtonPadding = 20;
constexpr int kMinMidiNote = 48;
constexpr int kMaxMidiNote = 72;
constexpr int kStatusAreaHeight = 110;

constexpr std::uint32_t kBackgroundColor = 0x05070C;
constexpr std::uint32_t kPanelColor = 0x0B1020;
constexpr std::uint32_t kPanelBorderColor = 0x394867;
constexpr std::uint32_t kMutedButtonColor = 0x1B1B1B;
constexpr std::uint32_t kMutedBorderColor = 0x555555;
constexpr std::uint32_t kMutedTextColor = 0xEAEAEA;
constexpr std::uint32_t kSelectedBorderColor = 0xF4F1DE;
constexpr std::uint32_t kSelectedTextColor = 0x081018;

}

void PerformanceUi::begin() {
  layout();
}

void PerformanceUi::drawInitial(const UiState& state) {
  M5.Display.fillScreen(kBackgroundColor);
  drawWaveformButtons(state);
  drawPerformanceBase();
  drawStatusArea(state);
  drawTouchMarker(state);
}

void PerformanceUi::refreshPerformance(const UiState& state) {
  drawStatusArea(state);
  drawTouchMarker(state);
}

void PerformanceUi::refreshWaveformSelection(Waveform previous, const UiState& state) {
  drawWaveformButton(previous, state);
  drawWaveformButton(state.selected_waveform, state);
  drawStatusArea(state);
}

bool PerformanceUi::isWaveformArea(int x) const {
  return x < performance_area_.x;
}

bool PerformanceUi::isPerformanceArea(int x, int y) const {
  return performance_area_.contains(x, y);
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
  const int note_span = kMaxMidiNote - kMinMidiNote;
  return kMinMidiNote + static_cast<int>(std::round(normalized * note_span));
}

void PerformanceUi::layout() {
  const int width = M5.Display.width();
  const int height = M5.Display.height();
  const int left_width = width / 2;
  performance_area_ = {left_width, 0, width - left_width, height};
  status_area_ = {performance_area_.x + 12, 12, performance_area_.w - 24, kStatusAreaHeight};

  const int available_height =
      height - (kWaveButtonPadding * 2) - (kWaveButtonGap * (static_cast<int>(Waveform::Count) - 1));
  const int button_height = std::min(kWaveButtonHeight, available_height / static_cast<int>(Waveform::Count));
  const int button_width = left_width - (kWaveButtonPadding * 2);

  for (int i = 0; i < static_cast<int>(Waveform::Count); ++i) {
    const int y = kWaveButtonPadding + i * (button_height + kWaveButtonGap);
    waveform_buttons_[static_cast<std::size_t>(i)] = {kWaveButtonPadding, y, button_width, button_height};
  }
}

void PerformanceUi::drawWaveformButtons(const UiState& state) {
  for (int i = 0; i < static_cast<int>(Waveform::Count); ++i) {
    drawWaveformButton(static_cast<Waveform>(i), state);
  }
}

void PerformanceUi::drawWaveformButton(Waveform waveform, const UiState& state) {
  const auto& rect = waveform_buttons_[static_cast<std::size_t>(waveform)];
  const bool selected = waveform == state.selected_waveform;
  const std::uint32_t fill = selected ? waveformColor(waveform) : kMutedButtonColor;
  const std::uint32_t border = selected ? kSelectedBorderColor : kMutedBorderColor;
  const std::uint32_t text = selected ? kSelectedTextColor : kMutedTextColor;

  M5.Display.fillRoundRect(rect.x, rect.y, rect.w, rect.h, 16, fill);
  M5.Display.drawRoundRect(rect.x, rect.y, rect.w, rect.h, 16, border);
  M5.Display.drawRoundRect(rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2, 16, border);
  M5.Display.setTextColor(text);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(waveformLabel(waveform), rect.x + rect.w / 2, rect.y + rect.h / 2);
}

void PerformanceUi::drawPerformanceBase() {
  M5.Display.fillRect(performance_area_.x, performance_area_.y, performance_area_.w, performance_area_.h,
                      kPanelColor);
  M5.Display.drawRect(performance_area_.x, performance_area_.y, performance_area_.w, performance_area_.h,
                      kPanelBorderColor);

  M5.Display.setTextColor(kMutedTextColor);
  M5.Display.setTextDatum(top_left);
  M5.Display.drawString("TOUCH AREA", performance_area_.x + 20, 18);
  M5.Display.drawString("X = PITCH", performance_area_.x + 20, 46);
  M5.Display.drawString("Y = RESERVED", performance_area_.x + 20, 74);
}

void PerformanceUi::drawStatusArea(const UiState& state) {
  M5.Display.fillRoundRect(status_area_.x, status_area_.y, status_area_.w, status_area_.h, 14, kBackgroundColor);
  M5.Display.drawRoundRect(status_area_.x, status_area_.y, status_area_.w, status_area_.h, 14, kPanelBorderColor);

  M5.Display.setTextColor(kMutedTextColor);
  M5.Display.setTextDatum(top_left);
  M5.Display.drawString(String("WAVE ") + waveformLabel(state.selected_waveform), status_area_.x + 16,
                        status_area_.y + 14);

  if (state.note_playing) {
    M5.Display.drawString(String("NOTE ") + state.active_midi_note, status_area_.x + 16, status_area_.y + 42);
    M5.Display.drawString(String(state.active_frequency, 1) + " Hz", status_area_.x + 16, status_area_.y + 70);
  } else {
    M5.Display.drawString("NOTE OFF", status_area_.x + 16, status_area_.y + 42);
    M5.Display.drawString("touch right panel", status_area_.x + 16, status_area_.y + 70);
  }
}

void PerformanceUi::drawTouchMarker(const UiState& state) {
  const int marker_radius = 18;
  const int marker_y = std::max(status_area_.y + status_area_.h + 24, performance_area_.y + 140);
  const Rect marker_zone = {performance_area_.x + 10, marker_y, performance_area_.w - 20,
                            performance_area_.h - marker_y - 10};

  M5.Display.fillRect(marker_zone.x, marker_zone.y, marker_zone.w, marker_zone.h, kPanelColor);

  if (!state.note_playing || state.last_touch_x < 0 || state.last_touch_y < 0) {
    return;
  }

  const int draw_x =
      std::clamp(state.last_touch_x, marker_zone.x + marker_radius, marker_zone.x + marker_zone.w - marker_radius);
  const int draw_y =
      std::clamp(state.last_touch_y, marker_zone.y + marker_radius, marker_zone.y + marker_zone.h - marker_radius);
  M5.Display.fillCircle(draw_x, draw_y, marker_radius, waveformColor(state.selected_waveform));
  M5.Display.drawCircle(draw_x, draw_y, marker_radius, kSelectedBorderColor);
}
