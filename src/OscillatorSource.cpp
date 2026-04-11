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
  buildFilteredWaveTables();
  setVolume(volume_);
  return true;
}

bool OscillatorSource::noteOn(std::size_t voice_index, float /*note_value*/, float frequency, Waveform waveform) {
  ensureFilteredWaveTables();
  M5.Speaker.tone(frequency, UINT32_MAX, channelForVoice(voice_index), true, waveformTable(waveform),
                  kWaveTableSize, false);
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

void OscillatorSource::setFilterEnabled(bool enabled) {
  if (filter_enabled_ == enabled) {
    return;
  }
  filter_enabled_ = enabled;
  filter_tables_dirty_ = true;
}

void OscillatorSource::setFilterParameters(float cutoff_normalized, float resonance_normalized, float mix_normalized) {
  const float next_cutoff = std::clamp(cutoff_normalized, 0.0f, 1.0f);
  const float next_resonance = std::clamp(resonance_normalized, 0.0f, 1.0f);
  const float next_mix = std::clamp(mix_normalized, 0.0f, 1.0f);
  if (std::fabs(next_cutoff - filter_cutoff_normalized_) < 0.0005f &&
      std::fabs(next_resonance - filter_resonance_normalized_) < 0.0005f &&
      std::fabs(next_mix - filter_mix_normalized_) < 0.0005f) {
    return;
  }
  filter_cutoff_normalized_ = next_cutoff;
  filter_resonance_normalized_ = next_resonance;
  filter_mix_normalized_ = next_mix;
  filter_tables_dirty_ = true;
}

void OscillatorSource::setDistortionEnabled(bool enabled) {
  if (distortion_enabled_ == enabled) {
    return;
  }
  distortion_enabled_ = enabled;
  filter_tables_dirty_ = true;
}

void OscillatorSource::setDistortionParameters(float drive_normalized, float tone_normalized, float mix_normalized) {
  const float next_drive = std::clamp(drive_normalized, 0.0f, 1.0f);
  const float next_tone = std::clamp(tone_normalized, 0.0f, 1.0f);
  const float next_mix = std::clamp(mix_normalized, 0.0f, 1.0f);
  if (std::fabs(next_drive - distortion_drive_normalized_) < 0.0005f &&
      std::fabs(next_tone - distortion_tone_normalized_) < 0.0005f &&
      std::fabs(next_mix - distortion_mix_normalized_) < 0.0005f) {
    return;
  }
  distortion_drive_normalized_ = next_drive;
  distortion_tone_normalized_ = next_tone;
  distortion_mix_normalized_ = next_mix;
  filter_tables_dirty_ = true;
}

void OscillatorSource::setBitcrusherEnabled(bool enabled) {
  if (bitcrusher_enabled_ == enabled) {
    return;
  }
  bitcrusher_enabled_ = enabled;
  filter_tables_dirty_ = true;
}

void OscillatorSource::setBitcrusherParameters(float bits_normalized, float rate_normalized, float mix_normalized) {
  const float next_bits = std::clamp(bits_normalized, 0.0f, 1.0f);
  const float next_rate = std::clamp(rate_normalized, 0.0f, 1.0f);
  const float next_mix = std::clamp(mix_normalized, 0.0f, 1.0f);
  if (std::fabs(next_bits - bitcrusher_bits_normalized_) < 0.0005f &&
      std::fabs(next_rate - bitcrusher_rate_normalized_) < 0.0005f &&
      std::fabs(next_mix - bitcrusher_mix_normalized_) < 0.0005f) {
    return;
  }
  bitcrusher_bits_normalized_ = next_bits;
  bitcrusher_rate_normalized_ = next_rate;
  bitcrusher_mix_normalized_ = next_mix;
  filter_tables_dirty_ = true;
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
  filter_tables_dirty_ = true;
}

void OscillatorSource::buildFilteredWaveTables() {
  auto build_filtered = [&](const auto& source, auto& destination) {
    std::array<float, kWaveTableSize> dry{};
    for (std::size_t i = 0; i < kWaveTableSize; ++i) {
      dry[i] = (static_cast<float>(source[i]) - SynthConfig::audio.center_sample) / SynthConfig::audio.center_sample;
    }

    std::array<float, kWaveTableSize> wet{};
    const float cutoff = std::clamp(filter_cutoff_normalized_, 0.0f, 1.0f);
    const float resonance = std::clamp(filter_resonance_normalized_, 0.0f, 1.0f);
    const float alpha = 0.02f + cutoff * 0.93f;
    const float feedback = 0.05f + resonance * 0.82f;

    float s1 = 0.0f;
    float s2 = 0.0f;
    float s3 = 0.0f;
    float s4 = 0.0f;
    for (std::size_t pass = 0; pass < 3; ++pass) {
      for (std::size_t i = 0; i < kWaveTableSize; ++i) {
        const float input = dry[i] - (s4 * feedback);
        s1 += alpha * (input - s1);
        s2 += alpha * (s1 - s2);
        s3 += alpha * (s2 - s3);
        s4 += alpha * (s3 - s4);
        wet[i] = std::clamp(s4, -1.0f, 1.0f);
      }
    }

    const bool use_filter = filter_enabled_ && filter_mix_normalized_ > 0.0f;
    const bool use_distortion = distortion_enabled_ && distortion_mix_normalized_ > 0.0f;
    const bool use_bitcrusher = bitcrusher_enabled_ && bitcrusher_mix_normalized_ > 0.0f;

    const float filter_mix = std::clamp(filter_mix_normalized_, 0.0f, 1.0f);
    const float drive_curve = std::pow(std::clamp(distortion_drive_normalized_, 0.0f, 1.0f), 0.80f);
    const float drive_gain = 1.0f + (drive_curve * 360.0f);
    const float distortion_mix = std::pow(std::clamp(distortion_mix_normalized_, 0.0f, 1.0f), 0.38f);
    const float tone_blend = std::clamp(distortion_tone_normalized_, 0.0f, 1.0f);
    const float bits_curve = std::pow(std::clamp(bitcrusher_bits_normalized_, 0.0f, 1.0f), 3.20f);
    const int quant_bits = 1 + static_cast<int>(std::lround(bits_curve * 5.0f));
    const int quant_steps = 1 << std::clamp(quant_bits, 1, 8);
    const float quant_scale = static_cast<float>(quant_steps - 1);
    const float rate_curve = std::pow(std::clamp(1.0f - bitcrusher_rate_normalized_, 0.0f, 1.0f), 1.20f);
    const int hold = 1 + static_cast<int>(std::lround(rate_curve * 511.0f));
    const float bitcrusher_mix = std::pow(std::clamp(bitcrusher_mix_normalized_, 0.0f, 1.0f), 0.35f);

    float crusher_hold_value = 0.0f;
    for (std::size_t i = 0; i < kWaveTableSize; ++i) {
      float sample = dry[i];
      if (use_filter) {
        sample = std::clamp(sample * (1.0f - filter_mix) + wet[i] * filter_mix, -1.0f, 1.0f);
      }

      if (use_distortion) {
        const float soft = std::tanh(sample * drive_gain);
        const float hard = std::clamp(sample * drive_gain * 1.35f, -1.0f, 1.0f);
        const float shaped = soft * (1.0f - tone_blend) + hard * tone_blend;
        const float leveled = std::clamp(shaped * (1.0f + 1.20f * drive_curve), -1.0f, 1.0f);
        const float toned = leveled * tone_blend + hard * (1.0f - tone_blend);
        sample = std::clamp(sample * (1.0f - distortion_mix) + toned * distortion_mix, -1.0f, 1.0f);
      }

      if (use_bitcrusher) {
        if ((i % std::max(1, hold)) == 0) {
          const float q = std::round((sample * 0.5f + 0.5f) * quant_scale) / quant_scale;
          crusher_hold_value = q * 2.0f - 1.0f;
        }
        sample = std::clamp(sample * (1.0f - bitcrusher_mix) + crusher_hold_value * bitcrusher_mix, -1.0f, 1.0f);
      }

      const float shifted = SynthConfig::audio.center_sample + sample * SynthConfig::audio.center_sample;
      destination[i] = static_cast<std::uint8_t>(std::lround(std::clamp(shifted, 1.0f, 255.0f)));
    }
  };

  build_filtered(sine_wave_, filtered_sine_wave_);
  build_filtered(saw_wave_, filtered_saw_wave_);
  build_filtered(square_wave_, filtered_square_wave_);
  build_filtered(triangle_wave_, filtered_triangle_wave_);
  filter_tables_dirty_ = false;
}

void OscillatorSource::ensureFilteredWaveTables() {
  if (!filter_tables_dirty_) {
    return;
  }
  buildFilteredWaveTables();
}

int OscillatorSource::channelForVoice(std::size_t voice_index) const {
  return SynthConfig::audio.audio_channel + static_cast<int>(voice_index);
}

const unsigned char* OscillatorSource::waveformTable(Waveform waveform) const {
  const bool use_effects = (filter_enabled_ && filter_mix_normalized_ > 0.0f) ||
                           (distortion_enabled_ && distortion_mix_normalized_ > 0.0f) ||
                           (bitcrusher_enabled_ && bitcrusher_mix_normalized_ > 0.0f);
  switch (waveform) {
    case Waveform::Sine:
      return use_effects ? filtered_sine_wave_.data() : sine_wave_.data();
    case Waveform::Saw:
      return use_effects ? filtered_saw_wave_.data() : saw_wave_.data();
    case Waveform::Square:
      return use_effects ? filtered_square_wave_.data() : square_wave_.data();
    case Waveform::Triangle:
      return use_effects ? filtered_triangle_wave_.data() : triangle_wave_.data();
    default:
      return use_effects ? filtered_sine_wave_.data() : sine_wave_.data();
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
