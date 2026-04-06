#include "AudioEngine.h"

#include <M5Unified.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace {

constexpr float kTwoPi = 6.28318530718f;

}

void AudioEngine::begin() {
  buildWaveTables();
  M5.Speaker.setVolume(SynthConfig::audio.speaker_volume);
  updateChannelVolume();
}

void AudioEngine::noteOn(int midi_note, Waveform waveform) {
  const float frequency = midiToFrequency(midi_note);
  if (note_playing_ && active_midi_note_ == midi_note && active_waveform_ == waveform) {
    return;
  }

  active_waveform_ = waveform;
  updateChannelVolume();
  M5.Speaker.tone(frequency, UINT32_MAX, SynthConfig::audio.audio_channel, true, waveformTable(waveform),
                  kWaveTableSize, false);

  note_playing_ = true;
  active_midi_note_ = midi_note;
  active_frequency_ = frequency;
}

void AudioEngine::noteOff() {
  M5.Speaker.stop(SynthConfig::audio.audio_channel);
  note_playing_ = false;
  active_midi_note_ = -1;
  active_frequency_ = 0.0f;
}

void AudioEngine::setVolume(float volume) {
  volume_ = std::clamp(volume, 0.0f, 1.0f);
  updateChannelVolume();
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

float AudioEngine::volume() const {
  return volume_;
}

void AudioEngine::buildWaveTables() {
  auto fill_table = [&](auto& table, Waveform waveform) {
    const float peak = SynthConfig::waveformPeak(waveform);
    for (std::size_t i = 0; i < kWaveTableSize; ++i) {
      const float phase = static_cast<float>(i) / static_cast<float>(kWaveTableSize);
      const float value = waveformValue(waveform, phase);
      const float sample = std::clamp(SynthConfig::audio.center_sample + value * peak, 1.0f, 255.0f);
      table[i] = static_cast<std::uint8_t>(std::lround(sample));
    }
  };

  fill_table(sine_wave_, Waveform::Sine);
  fill_table(saw_wave_, Waveform::Saw);
  fill_table(square_wave_, Waveform::Square);
  fill_table(triangle_wave_, Waveform::Triangle);
}

void AudioEngine::updateChannelVolume() const {
  const float scaled = std::clamp(volume_ * SynthConfig::waveformTrim(active_waveform_), 0.0f, 1.0f);
  M5.Speaker.setChannelVolume(SynthConfig::audio.audio_channel,
                              static_cast<std::uint8_t>(std::lround(scaled * 255.0f)));
}

float AudioEngine::midiToFrequency(int midi_note) {
  return 440.0f * std::pow(2.0f, (static_cast<float>(midi_note) - 69.0f) / 12.0f);
}

const unsigned char* AudioEngine::waveformTable(Waveform waveform) const {
  switch (waveform) {
    case Waveform::Sine:
      return sine_wave_.data();
    case Waveform::Saw:
      return saw_wave_.data();
    case Waveform::Square:
      return square_wave_.data();
    case Waveform::Triangle:
      return triangle_wave_.data();
    default:
      return sine_wave_.data();
  }
}

float AudioEngine::waveformValue(Waveform waveform, float phase) {
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
