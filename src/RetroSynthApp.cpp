#include "RetroSynthApp.h"

#include "AudioSourceType.h"
#include "SynthConfig.h"

#include <Arduino.h>

#include <algorithm>
#include <cmath>

namespace {

enum Target {
  Volume,
  Attack,
  Decay,
  Sustain,
  Release,
  DelayTime,
  DelayFeedback,
  DelayMix,
  ChorusRate,
  ChorusDepth,
  ChorusMix,
  DistortionDrive,
  DistortionTone,
  DistortionMix,
  BitcrusherBits,
  BitcrusherRate,
  BitcrusherMix,
};

enum Effect {
  Delay,
  Chorus,
  Distortion,
  Bitcrusher,
};

constexpr float kTwoPi = 6.28318530718f;

float clampUnit(float value) {
  return std::clamp(value, 0.0f, 1.0f);
}

}  // namespace

void RetroSynthApp::begin() {
  audio_engine_.begin();
  if (!lvgl_.begin()) {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_RED, TFT_BLACK);
    M5.Display.drawString("LVGL buffer allocation failed", 32, 32);
    return;
  }

  const lvgl_synth_callbacks_t callbacks{
      stateChanged,
      noteChanged,
      xyNoteChanged,
      micRecordingChanged,
      this,
  };
  lvgl_synth_ui_set_callbacks(&callbacks);
  lvgl_synth_ui_set_theme(LVGL_SYNTH_THEME_RETRO_WOOD);
  lvgl_synth_ui_create();
  readUiState();
  applyAudioState(false);
  lvgl_.update();
}

void RetroSynthApp::update() {
  audio_engine_.update();
  const std::uint32_t now = millis();
  if (rendering_suspended_ && !note_pressed_ && static_cast<std::int32_t>(now - render_resume_ms_) >= 0) {
    lvgl_synth_ui_invalidate_performance();
    lvgl_.setRenderingEnabled(true);
    rendering_suspended_ = false;
  }
  if (mic_recording_) {
    audio_engine_.updateMicSampleRecording();
    if (millis() - mic_started_ms_ >= SynthConfig::audio.mic_sample_max_ms) {
      audio_engine_.finishMicSampleRecording(true);
      mic_recording_ = false;
      readUiState();
      applyAudioState(false);
    }
  }

  if (hasActiveLfo() && now - last_lfo_update_ms_ >= 10) {
    applyAudioState(true);
    last_lfo_update_ms_ = now;
  }
  lvgl_.update();
  updatePressedNotes();
}

void RetroSynthApp::stateChanged(void* user_data) {
  auto* app = static_cast<RetroSynthApp*>(user_data);
  app->readUiState();
  app->applyAudioState(false);
}

void RetroSynthApp::noteChanged(float midi_note, bool pressed, void* user_data) {
  auto* app = static_cast<RetroSynthApp*>(user_data);
  if (app->mic_recording_) {
    return;
  }
  const int note = std::clamp(static_cast<int>(std::lround(midi_note)), 0, 127);
  app->pressed_notes_[note] = pressed;
  app->performance_input_dirty_ = true;
}

void RetroSynthApp::xyNoteChanged(float midi_note, bool pressed, void* user_data) {
  auto* app = static_cast<RetroSynthApp*>(user_data);
  if (app->mic_recording_) {
    return;
  }
  if (!pressed) {
    app->xy_note_active_ = false;
    app->performance_input_dirty_ = true;
    return;
  }
  app->performance_input_dirty_ = true;
  if (app->xy_note_active_ && std::fabs(app->xy_note_ - midi_note) < 0.25f) {
    return;
  }
  app->xy_note_ = midi_note;
  app->xy_note_active_ = true;
}

void RetroSynthApp::updatePressedNotes() {
  if (!performance_input_dirty_) {
    return;
  }
  performance_input_dirty_ = false;

  // Key press/release objects are already invalidated by the LVGL callbacks.
  // Their first invalidation can be consumed by a suppressed refresh, so
  // invalidate the exact changed-key rectangles again before the forced draw.
  lvgl_synth_ui_invalidate_dirty_keys();
  lvgl_synth_ui_invalidate_dirty_xy();
  lvgl_.renderNow();

  std::array<float, SynthConfig::audio.polyphony_voices> notes{};
  std::size_t count = 0;
  if (xy_note_active_) {
    notes[count++] = xy_note_;
  }
  for (std::size_t note = 0; note < pressed_notes_.size() && count < notes.size(); ++note) {
    if (pressed_notes_[note]) {
      notes[count++] = static_cast<float>(note);
    }
  }

  if (count == 0) {
    note_pressed_ = false;
    audio_engine_.noteOff();
    const float release = static_cast<float>(ui_state_.values[Release]) / 100.0f;
    render_resume_ms_ = millis() + static_cast<std::uint32_t>(release * SynthConfig::audio.amp_release_max_ms) + 50;
    return;
  }

  active_note_ = notes[0];
  note_pressed_ = true;
  lvgl_.setRenderingEnabled(false);
  rendering_suspended_ = true;
  audio_engine_.noteOnVoices(notes.data(), count, selectedWaveform());
}

void RetroSynthApp::micRecordingChanged(bool recording, void* user_data) {
  auto* app = static_cast<RetroSynthApp*>(user_data);
  if (recording) {
    app->audio_engine_.noteOff();
    app->note_pressed_ = false;
    app->mic_recording_ = app->audio_engine_.beginMicSampleRecording();
    app->mic_started_ms_ = millis();
    return;
  }

  if (!app->mic_recording_) {
    return;
  }
  const bool commit = millis() - app->mic_started_ms_ >= SynthConfig::audio.mic_sample_commit_min_ms;
  app->audio_engine_.finishMicSampleRecording(commit);
  app->mic_recording_ = false;
  app->readUiState();
  app->applyAudioState(false);
}

void RetroSynthApp::readUiState() {
  lvgl_synth_ui_get_state(&ui_state_);
}

void RetroSynthApp::applyAudioState(bool include_lfo) {
  const std::uint32_t now = millis();
  const auto value = [this, include_lfo, now](int target) {
    return include_lfo ? modulatedValue(target, now) : static_cast<float>(ui_state_.values[target]) / 100.0f;
  };

  if (ui_state_.source <= 3) {
    audio_engine_.setSourceType(AudioSourceType::Oscillator);
  } else if (ui_state_.source == 4) {
    if (audio_engine_.hasMicSample()) {
      audio_engine_.setSourceType(AudioSourceType::OnboardMic);
    }
  } else {
    audio_engine_.setSourceType(AudioSourceType::ExternalI2S);
  }

  audio_engine_.setVolume(value(Volume));
  audio_engine_.setAttackNormalized(value(Attack));
  audio_engine_.setDecayNormalized(value(Decay));
  audio_engine_.setSustainNormalized(value(Sustain));
  audio_engine_.setReleaseNormalized(value(Release));

  audio_engine_.setDelayEnabled(ui_state_.effect_enabled[Delay]);
  audio_engine_.setDelayParameters(value(DelayTime), value(DelayFeedback), value(DelayMix));
  audio_engine_.setChorusEnabled(ui_state_.effect_enabled[Chorus]);
  audio_engine_.setChorusParameters(value(ChorusRate), value(ChorusDepth), value(ChorusMix));
  audio_engine_.setDistortionEnabled(ui_state_.effect_enabled[Distortion]);
  audio_engine_.setDistortionParameters(value(DistortionDrive), value(DistortionTone), value(DistortionMix));
  audio_engine_.setBitcrusherEnabled(ui_state_.effect_enabled[Bitcrusher]);
  audio_engine_.setBitcrusherParameters(value(BitcrusherBits), value(BitcrusherRate), value(BitcrusherMix));
  audio_engine_.setFilterEnabled(false);
}

bool RetroSynthApp::hasActiveLfo() const {
  for (int target = 0; target < LVGL_SYNTH_TARGET_COUNT; ++target) {
    if (ui_state_.lfo_enabled[target] && ui_state_.lfo_depth[target] > 0) {
      return true;
    }
  }
  return false;
}

float RetroSynthApp::modulatedValue(int target, std::uint32_t now_ms) const {
  const float base = static_cast<float>(ui_state_.values[target]) / 100.0f;
  if (!ui_state_.lfo_enabled[target] || ui_state_.lfo_depth[target] == 0) {
    return base;
  }

  const float rate = 0.05f + 9.95f * static_cast<float>(ui_state_.lfo_rate[target]) / 100.0f;
  const float phase = std::fmod(static_cast<float>(now_ms) * rate / 1000.0f, 1.0f);
  float wave = 0.0f;
  if (ui_state_.lfo_wave[target] < 34) {
    wave = std::sin(kTwoPi * phase);
  } else if (ui_state_.lfo_wave[target] < 67) {
    wave = 1.0f - 4.0f * std::fabs(phase - 0.5f);
  } else {
    wave = phase < 0.5f ? 1.0f : -1.0f;
  }
  const float depth = static_cast<float>(ui_state_.lfo_depth[target]) / 100.0f;
  return clampUnit(base + wave * depth * 0.5f);
}

Waveform RetroSynthApp::selectedWaveform() const {
  if (ui_state_.source <= 3) {
    return static_cast<Waveform>(ui_state_.source);
  }
  return Waveform::Sine;
}
