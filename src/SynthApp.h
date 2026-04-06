#pragma once

#include "AudioEngine.h"
#include "PerformanceUi.h"

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
  void handleVolumeTouch(int x, int y);
  void handlePerformanceTouches(const std::array<int, SynthConfig::ui.max_touch_points>& xs,
                                const std::array<int, SynthConfig::ui.max_touch_points>& ys,
                                std::size_t count);
  void finishMicRecording();
  void syncUiState();
  void stopNote();

  AudioEngine audio_engine_{};
  PerformanceUi ui_{};
  UiState ui_state_{};
  bool mic_recording_ = false;
  std::uint32_t mic_recording_started_ms_ = 0;
  std::uint32_t last_ui_refresh_ms_ = 0;
  std::size_t last_drawn_touch_count_ = 0;
  std::array<int, SynthConfig::ui.max_touch_points> last_drawn_touch_xs_{};
  std::array<int, SynthConfig::ui.max_touch_points> last_drawn_touch_ys_{};
};
