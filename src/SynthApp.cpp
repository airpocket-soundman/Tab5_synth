#include "SynthApp.h"

#include <M5Unified.h>

#include <array>
#include <cstdlib>

namespace {

constexpr std::uint32_t kUiRefreshIntervalMs = 33;
constexpr int kTouchMarkerThreshold = 12;
constexpr float kVolumeChangeThreshold = 0.01f;

bool touchMovedEnough(int current_x, int current_y, int previous_x, int previous_y) {
  return std::abs(current_x - previous_x) >= kTouchMarkerThreshold ||
         std::abs(current_y - previous_y) >= kTouchMarkerThreshold;
}

bool isTouchActive(const m5::touch_detail_t& touch) {
  return touch.isPressed() || touch.wasPressed() || touch.isHolding();
}

}  // namespace

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
  bool mic_button_active = false;

  if (count > 0) {
    for (std::uint8_t i = 0; i < count && i < SynthConfig::ui.max_touch_points; ++i) {
      const auto& touch = M5.Touch.getDetail(i);
      const int x = touch.x;
      const int y = touch.y;
      if (isTouchActive(touch) && ui_.isSelectionArea(x, y) &&
          ui_.sourceAt(x, y, ui_state_.selected_source) == AudioSourceType::OnboardMic) {
        mic_button_active = true;
        break;
      }
    }
  }

  if (mic_recording_) {
    if (mic_button_active) {
      audio_engine_.updateMicSampleRecording();
    } else {
      finishMicRecording();
    }
    return;
  }

  if (count == 0) {
    if (audio_engine_.isNotePlaying() || ui_state_.touch_count > 0) {
      stopNote();
    }
    return;
  }

  std::array<int, SynthConfig::ui.max_touch_points> perf_xs{};
  std::array<int, SynthConfig::ui.max_touch_points> perf_ys{};
  std::size_t perf_count = 0;

  for (std::uint8_t i = 0; i < count && i < SynthConfig::ui.max_touch_points; ++i) {
    const auto& touch = M5.Touch.getDetail(i);
    const int x = touch.x;
    const int y = touch.y;

    if (touch.wasPressed() && ui_.isSelectionArea(x, y)) {
      handleSelectionTouch(x, y);
      return;
    }

    if (touch.wasPressed() && ui_.isParameterArea(x, y)) {
      ui_state_.selected_parameter = ui_.parameterAt(x, y, ui_state_.selected_parameter);
      ui_.refreshParameterSelection(ui_state_);
      last_ui_refresh_ms_ = millis();
      return;
    }

    if (touch.wasPressed() && ui_.isPitchModeArea(x, y)) {
      handlePitchModeTouch(x, y);
      return;
    }

    if (isTouchActive(touch) && ui_.isVolumeArea(x, y)) {
      handleVolumeTouch(x, y);
      return;
    }

    if (isTouchActive(touch) && ui_.isPerformanceArea(x, y)) {
      perf_xs[perf_count] = x;
      perf_ys[perf_count] = y;
      ++perf_count;
    }
  }

  if (perf_count > 0) {
    handlePerformanceTouches(perf_xs, perf_ys, perf_count);
  } else if (audio_engine_.isNotePlaying() || ui_state_.touch_count > 0) {
    stopNote();
  }
}

void SynthApp::handleSelectionTouch(int x, int y) {
  const AudioSourceType next_source = ui_.sourceAt(x, y, ui_state_.selected_source);
  const AudioSourceType previous_source = ui_state_.selected_source;
  const Waveform previous_waveform = ui_state_.selected_waveform;

  if (next_source == AudioSourceType::OnboardMic) {
    stopNote();
    mic_recording_ = audio_engine_.beginMicSampleRecording();
    if (mic_recording_) {
      mic_recording_started_ms_ = millis();
    }
  } else {
    if (next_source == AudioSourceType::Oscillator) {
      ui_state_.selected_waveform = ui_.waveformAt(x, y, ui_state_.selected_waveform);
    }
    audio_engine_.setSourceType(next_source);
  }

  syncUiState();

  if (previous_source != ui_state_.selected_source || previous_waveform != ui_state_.selected_waveform) {
    ui_.refreshSourceSelection(ui_state_);
  }
  ui_.refreshPerformance(ui_state_);
  last_ui_refresh_ms_ = millis();
}

void SynthApp::handlePitchModeTouch(int x, int y) {
  const bool next_quantize = ui_.quantizeModeAt(x, y, ui_state_.quantize_to_semitone);
  if (next_quantize == ui_state_.quantize_to_semitone) {
    return;
  }

  ui_state_.quantize_to_semitone = next_quantize;
  ui_.refreshPitchMode(ui_state_);
}

void SynthApp::handleVolumeTouch(int x, int /*y*/) {
  const float next_volume = ui_.volumeFromTouch(x);
  if (std::abs(next_volume - ui_state_.volume) < kVolumeChangeThreshold) {
    return;
  }

  audio_engine_.setVolume(next_volume);
  syncUiState();
  ui_.refreshVolumeControl(ui_state_);
  last_ui_refresh_ms_ = millis();
}

void SynthApp::handlePerformanceTouches(const std::array<int, SynthConfig::ui.max_touch_points>& xs,
                                        const std::array<int, SynthConfig::ui.max_touch_points>& ys,
                                        std::size_t count) {
  std::array<float, SynthConfig::ui.max_touch_points> note_values{};
  for (std::size_t i = 0; i < count; ++i) {
    note_values[i] = ui_.xToNoteValue(xs[i], ui_state_.quantize_to_semitone);
  }

  bool marker_changed = count != last_drawn_touch_count_;
  for (std::size_t i = 0; i < count && !marker_changed; ++i) {
    marker_changed = touchMovedEnough(xs[i], ys[i], last_drawn_touch_xs_[i], last_drawn_touch_ys_[i]);
  }
  const bool refresh_due = (millis() - last_ui_refresh_ms_) >= kUiRefreshIntervalMs;

  audio_engine_.noteOnVoices(note_values.data(), count, ui_state_.selected_waveform);
  ui_state_.touch_count = count;
  ui_state_.touch_xs = xs;
  ui_state_.touch_ys = ys;
  syncUiState();
  ui_state_.touch_count = count;
  ui_state_.touch_xs = xs;
  ui_state_.touch_ys = ys;

  if (marker_changed || refresh_due) {
    ui_.refreshPerformance(ui_state_);
    last_ui_refresh_ms_ = millis();
    last_drawn_touch_count_ = count;
    last_drawn_touch_xs_ = xs;
    last_drawn_touch_ys_ = ys;
  }
}

void SynthApp::finishMicRecording() {
  mic_recording_ = false;
  const std::uint32_t held_ms = millis() - mic_recording_started_ms_;
  const bool commit_sample = held_ms >= SynthConfig::audio.mic_sample_commit_min_ms;
  audio_engine_.finishMicSampleRecording(commit_sample);
  mic_recording_started_ms_ = 0;
  syncUiState();
  ui_.refreshSourceSelection(ui_state_);
  ui_.refreshPerformance(ui_state_);
  last_ui_refresh_ms_ = millis();
}

void SynthApp::syncUiState() {
  ui_state_.selected_source = audio_engine_.activeSourceType();
  ui_state_.oscillator_available = audio_engine_.isSourceAvailable(AudioSourceType::Oscillator);
  ui_state_.onboard_mic_available = audio_engine_.isSourceAvailable(AudioSourceType::OnboardMic);
  ui_state_.external_i2s_available = audio_engine_.isSourceAvailable(AudioSourceType::ExternalI2S);
  ui_state_.note_playing = audio_engine_.isNotePlaying();
  ui_state_.active_midi_note = audio_engine_.activeMidiNote();
  ui_state_.active_frequency = audio_engine_.activeFrequency();
  ui_state_.volume = audio_engine_.volume();

  if (!ui_state_.note_playing && ui_state_.touch_count == 0) {
    ui_state_.touch_xs.fill(0);
    ui_state_.touch_ys.fill(0);
  }
}

void SynthApp::stopNote() {
  audio_engine_.noteOff();
  ui_state_.touch_count = 0;
  ui_state_.touch_xs.fill(0);
  ui_state_.touch_ys.fill(0);
  syncUiState();
  ui_.refreshPerformance(ui_state_);
  last_ui_refresh_ms_ = millis();
  last_drawn_touch_count_ = 0;
  last_drawn_touch_xs_.fill(0);
  last_drawn_touch_ys_.fill(0);
}
