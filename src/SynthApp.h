#pragma once

#include "AudioEngine.h"
#include "PerformanceUi.h"

#include <cstdint>

class SynthApp {
 public:
  void begin();
  void update();

 private:
  void handleTouch();
  void handleWaveformTouch(int x, int y);
  void handlePerformanceTouch(int x, int y);
  void syncUiState();
  void stopNote();

  AudioEngine audio_engine_{};
  PerformanceUi ui_{};
  UiState ui_state_{};
  std::uint32_t last_ui_refresh_ms_ = 0;
  int last_drawn_touch_x_ = -1;
  int last_drawn_touch_y_ = -1;
};
