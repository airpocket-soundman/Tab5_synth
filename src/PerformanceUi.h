#pragma once

#include "Rect.h"
#include "Waveform.h"

#include <M5Unified.h>

#include <array>
#include <cstddef>

struct UiState {
  Waveform selected_waveform = Waveform::Sine;
  bool note_playing = false;
  int active_midi_note = -1;
  float active_frequency = 0.0f;
  int last_touch_x = -1;
  int last_touch_y = -1;
};

class PerformanceUi {
 public:
  void begin();
  void drawInitial(const UiState& state);
  void refreshPerformance(const UiState& state);
  void refreshWaveformSelection(Waveform previous, const UiState& state);

  [[nodiscard]] bool isWaveformArea(int x) const;
  [[nodiscard]] bool isPerformanceArea(int x, int y) const;
  [[nodiscard]] Waveform waveformAt(int x, int y, Waveform fallback) const;
  [[nodiscard]] int xToMidiNote(int x) const;

 private:
  void layout();
  void drawWaveformButtons(const UiState& state);
  void drawWaveformButton(Waveform waveform, const UiState& state);
  void drawPerformanceBase();
  void drawStatusArea(const UiState& state);
  void drawTouchMarker(const UiState& state);

  std::array<Rect, static_cast<std::size_t>(Waveform::Count)> waveform_buttons_{};
  Rect performance_area_{};
  Rect status_area_{};
};
