#include "OscillatorSource.h"

#include <M5Unified.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace {

constexpr float kTwoPi = 6.28318530718f;

}

bool OscillatorSource::begin() {
  buildWaveTables();
  setVolume(volume_);
  return true;
}

bool OscillatorSource::noteOn(std::size_t voice_index, float /*note_value*/, float frequency, Waveform waveform) {
  M5.Speaker.tone(frequency, UINT32_MAX, channelForVoice(voice_index), true, waveformTable(waveform),
                  kWaveTableSize, false);
  setVoiceLevel(voice_index, volume_, waveform);
  return true;
}

void OscillatorSource::noteOff(std::size_t voice_index) {
  M5.Speaker.stop(channelForVoice(voice_index));
}

void OscillatorSource::noteOffAll() {
  for (std::size_t i = 0; i < SynthConfig::audio.polyphony_voices; ++i) {
    noteOff(i);
  }
}

void OscillatorSource::setVolume(float volume) {
  volume_ = std::clamp(volume, 0.0f, 1.0f);
}

void OscillatorSource::setVoiceLevel(std::size_t voice_index, float level, Waveform waveform) {
  const float scaled = std::clamp(level * SynthConfig::waveformTrim(waveform), 0.0f, 1.0f);
  M5.Speaker.setChannelVolume(channelForVoice(voice_index),
                              static_cast<std::uint8_t>(std::lround(scaled * 255.0f)));
}

bool OscillatorSource::isAvailable() const {
  return true;
}

AudioSourceType OscillatorSource::type() const { return AudioSourceType::Oscillator; }

void OscillatorSource::buildWaveTables() {
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

int OscillatorSource::channelForVoice(std::size_t voice_index) const {
  return SynthConfig::audio.audio_channel + static_cast<int>(voice_index);
}

const unsigned char* OscillatorSource::waveformTable(Waveform waveform) const {
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

float OscillatorSource::waveformValue(Waveform waveform, float phase) {
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
