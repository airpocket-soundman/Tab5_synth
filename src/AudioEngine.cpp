#include "AudioEngine.h"

#include <M5Unified.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace {

constexpr int kSpeakerVolume = 180;
constexpr std::uint32_t kSampleRate = 24000;
constexpr std::size_t kBufferSamples = 256;
constexpr int kAudioChannel = 0;
constexpr float kAttackStep = 0.020f;
constexpr float kReleaseStep = 0.010f;
constexpr float kAmplitude = 14000.0f;
constexpr float kTwoPi = 6.28318530718f;

}

void AudioEngine::begin() {
  M5.Speaker.setVolume(kSpeakerVolume);
  M5.Speaker.setChannelVolume(kAudioChannel, 255);

  if (task_handle_ == nullptr) {
    xTaskCreatePinnedToCore(audioTaskEntry, "audio_engine", 4096, this, 3, &task_handle_, 0);
  }
}

void AudioEngine::noteOn(int midi_note, Waveform waveform) {
  const float frequency = midiToFrequency(midi_note);

  taskENTER_CRITICAL(&mutex_);
  render_state_.gate = true;
  render_state_.frequency = frequency;
  render_state_.waveform = waveform;
  taskEXIT_CRITICAL(&mutex_);

  note_playing_ = true;
  active_midi_note_ = midi_note;
  active_frequency_ = frequency;
  active_waveform_ = waveform;
}

void AudioEngine::noteOff() {
  taskENTER_CRITICAL(&mutex_);
  render_state_.gate = false;
  taskEXIT_CRITICAL(&mutex_);

  note_playing_ = false;
  active_midi_note_ = -1;
  active_frequency_ = 0.0f;
}

bool AudioEngine::isNotePlaying() const {
  return note_playing_;
}

int AudioEngine::activeMidiNote() const {
  return active_midi_note_;
}

float AudioEngine::activeFrequency() const {
  return active_frequency_;
}

Waveform AudioEngine::activeWaveform() const {
  return active_waveform_;
}

void AudioEngine::audioTaskEntry(void* arg) {
  static_cast<AudioEngine*>(arg)->audioTaskLoop();
}

void AudioEngine::audioTaskLoop() {
  std::array<std::array<std::int16_t, kBufferSamples>, 2> buffers{};
  std::size_t buffer_index = 0;

  for (;;) {
    RenderState state;
    taskENTER_CRITICAL(&mutex_);
    state = render_state_;
    taskEXIT_CRITICAL(&mutex_);

    fillBuffer(buffers[buffer_index].data(), kBufferSamples);

    const bool should_output = level_ > 0.0005f || state.gate;
    if (should_output) {
      while (M5.Speaker.isPlaying(kAudioChannel) >= 2) {
        vTaskDelay(1);
      }
      M5.Speaker.playRaw(buffers[buffer_index].data(), kBufferSamples, kSampleRate, false, 1, kAudioChannel, false);
      buffer_index = (buffer_index + 1) & 1;
    } else {
      vTaskDelay(1);
    }
  }
}

void AudioEngine::fillBuffer(std::int16_t* buffer, std::size_t sample_count) {
  RenderState state;
  taskENTER_CRITICAL(&mutex_);
  state = render_state_;
  taskEXIT_CRITICAL(&mutex_);

  const float phase_step = state.frequency / static_cast<float>(kSampleRate);

  for (std::size_t i = 0; i < sample_count; ++i) {
    const float target_level = state.gate ? 1.0f : 0.0f;
    const float step = state.gate ? kAttackStep : kReleaseStep;
    level_ += (target_level - level_) * step;

    const float sample = waveformSample(state.waveform, phase_) * level_;
    buffer[i] = static_cast<std::int16_t>(std::clamp(sample * kAmplitude, -32767.0f, 32767.0f));

    phase_ += phase_step;
    if (phase_ >= 1.0f) {
      phase_ -= std::floor(phase_);
    }
  }
}

float AudioEngine::midiToFrequency(int midi_note) {
  return 440.0f * std::pow(2.0f, (static_cast<float>(midi_note) - 69.0f) / 12.0f);
}

float AudioEngine::waveformSample(Waveform waveform, float phase) {
  switch (waveform) {
    case Waveform::Sine:
      return std::sinf(phase * kTwoPi);
    case Waveform::Saw:
      return (2.0f * phase) - 1.0f;
    case Waveform::Square:
      return phase < 0.5f ? 1.0f : -1.0f;
    case Waveform::Triangle:
      return 1.0f - 4.0f * std::fabs(phase - 0.5f);
    default:
      return 0.0f;
  }
}
