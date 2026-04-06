#include "SynthApp.h"

#include <M5Unified.h>

#include <cstdlib>

namespace {

constexpr std::uint32_t kUiRefreshIntervalMs = 33;
constexpr int kTouchMarkerThreshold = 12;
constexpr float kVolumeChangeThreshold = 0.01f;

}

void SynthApp::begin() {
  ui_.begin();
  audio_engine_.begin();
  syncUiState();
  ui_.drawInitial(ui_state_);
  last_ui_refresh_ms_ = millis();
}

void SynthApp::update() {
  handleTouch();
}

void SynthApp::handleTouch() {
  const std::uint8_t count = M5.Touch.getCount();
  if (count == 0) {
    if (audio_engine_.isNotePlaying()) {
      stopNote();
    }
    return;
  }

  const auto& touch = M5.Touch.getDetail(0);
  const int x = touch.x;
  const int y = touch.y;

  if (ui_.isWaveformArea(x)) {
    if (touch.wasPressed()) {
      handleWaveformTouch(x, y);
    }
    if (touch.isPressed() || touch.wasPressed() || touch.isHolding()) {
      handleVolumeTouch(x, y);
    }
    return;
  }

  if (touch.isPressed() || touch.wasPressed() || touch.isHolding()) {
    handlePerformanceTouch(x, y);
    return;
  }

  if (touch.wasReleased() && audio_engine_.isNotePlaying()) {
    stopNote();
  }
}

void SynthApp::handleWaveformTouch(int x, int y) {
  if (ui_.isVolumeArea(x, y)) {
    return;
  }

  const Waveform next_waveform = ui_.waveformAt(x, y, ui_state_.selected_waveform);
  if (next_waveform == ui_state_.selected_waveform) {
    return;
  }

  const Waveform previous = ui_state_.selected_waveform;
  ui_state_.selected_waveform = next_waveform;
  syncUiState();
  ui_.refreshWaveformSelection(previous, ui_state_);

  if (audio_engine_.isNotePlaying()) {
    audio_engine_.noteOn(audio_engine_.activeMidiNote(), ui_state_.selected_waveform);
    syncUiState();
    ui_.refreshPerformance(ui_state_);
    last_ui_refresh_ms_ = millis();
  }
}

void SynthApp::handleVolumeTouch(int x, int y) {
  if (!ui_.isVolumeArea(x, y)) {
    return;
  }

  const float next_volume = ui_.volumeFromTouch(x);
  if (std::abs(next_volume - ui_state_.volume) < kVolumeChangeThreshold) {
    return;
  }

  audio_engine_.setVolume(next_volume);
  syncUiState();
  ui_.refreshVolumeControl(ui_state_);
  last_ui_refresh_ms_ = millis();
}

void SynthApp::handlePerformanceTouch(int x, int y) {
  if (!ui_.isPerformanceArea(x, y)) {
    if (audio_engine_.isNotePlaying()) {
      stopNote();
    }
    return;
  }

  const int midi_note = ui_.xToMidiNote(x);
  const bool note_changed = !audio_engine_.isNotePlaying() || midi_note != audio_engine_.activeMidiNote();
  const bool marker_changed = std::abs(x - last_drawn_touch_x_) >= kTouchMarkerThreshold ||
                              std::abs(y - last_drawn_touch_y_) >= kTouchMarkerThreshold;
  const bool refresh_due = (millis() - last_ui_refresh_ms_) >= kUiRefreshIntervalMs;

  audio_engine_.noteOn(midi_note, ui_state_.selected_waveform);
  ui_state_.last_touch_x = x;
  ui_state_.last_touch_y = y;
  syncUiState();

  if (note_changed || (marker_changed && refresh_due)) {
    ui_.refreshPerformance(ui_state_);
    last_ui_refresh_ms_ = millis();
    last_drawn_touch_x_ = x;
    last_drawn_touch_y_ = y;
  }
}

void SynthApp::syncUiState() {
  ui_state_.note_playing = audio_engine_.isNotePlaying();
  ui_state_.active_midi_note = audio_engine_.activeMidiNote();
  ui_state_.active_frequency = audio_engine_.activeFrequency();
  ui_state_.volume = audio_engine_.volume();

  if (!ui_state_.note_playing) {
    ui_state_.last_touch_x = -1;
    ui_state_.last_touch_y = -1;
  }
}

void SynthApp::stopNote() {
  audio_engine_.noteOff();
  syncUiState();
  ui_.refreshPerformance(ui_state_);
  last_ui_refresh_ms_ = millis();
  last_drawn_touch_x_ = -1;
  last_drawn_touch_y_ = -1;
}
