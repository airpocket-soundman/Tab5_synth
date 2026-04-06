#pragma once

#include "Rect.h"
#include "SynthConfig.h"
#include "Waveform.h"

#include <M5Unified.h>

#include <array>
#include <cstddef>

struct UiState {
  Waveform selected_waveform = Waveform::Sine;
  bool note_playing = false;
  int active_midi_note = -1;
  float active_frequency = 0.0f;
  float volume = SynthConfig::audio.default_volume;
  int last_touch_x = -1;
  int last_touch_y = -1;
};

class PerformanceUi {
 public:
  void begin();
  void drawInitial(const UiState& state);
  void refreshPerformance(const UiState& state);
  void refreshWaveformSelection(Waveform previous, const UiState& state);
  void refreshVolumeControl(const UiState& state);

  [[nodiscard]] bool isWaveformArea(int x) const;
  [[nodiscard]] bool isPerformanceArea(int x, int y) const;
  [[nodiscard]] bool isVolumeArea(int x, int y) const;
  [[nodiscard]] Waveform waveformAt(int x, int y, Waveform fallback) const;
  [[nodiscard]] int xToMidiNote(int x) const;
  [[nodiscard]] float volumeFromTouch(int x) const;

 private:
  [[nodiscard]] int normalizedTouchX(int x) const;
  [[nodiscard]] int normalizedTouchY(int y) const;
  void layout();
  void drawWaveformButtons(const UiState& state);
  void drawWaveformButton(Waveform waveform, const UiState& state);
  void drawWaveformIcon(const Rect& rect, Waveform waveform, std::uint32_t color);
  void drawVolumeControl(const UiState& state);
  void drawPerformanceBase();
  void drawCoordinateText(const UiState& state);
  void drawTouchMarker(const UiState& state);
  void drawCenterGuides(const Rect& bounds);

  std::array<Rect, static_cast<std::size_t>(Waveform::Count)> waveform_buttons_{};
  Rect volume_area_{};
  Rect performance_area_{};
  Rect coordinate_text_area_{};
};
