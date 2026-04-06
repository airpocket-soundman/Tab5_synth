#pragma once

#include "AudioSource.h"
#include "SynthConfig.h"

#include <array>
#include <cstddef>
#include <cstdint>

class OscillatorSource : public AudioSource {
 public:
  bool begin() override;
  bool noteOn(std::size_t voice_index, float note_value, float frequency, Waveform waveform) override;
  void noteOff(std::size_t voice_index) override;
  void noteOffAll() override;
  void setVolume(float volume) override;
  bool isAvailable() const override;
  AudioSourceType type() const override;

 private:
  static constexpr std::size_t kWaveTableSize = SynthConfig::audio.wavetable_size;

  void buildWaveTables();
  void updateChannelVolume(std::size_t voice_index, Waveform waveform) const;
  int channelForVoice(std::size_t voice_index) const;
  const unsigned char* waveformTable(Waveform waveform) const;
  static float waveformValue(Waveform waveform, float phase);

  float volume_ = SynthConfig::audio.default_volume;
  std::array<std::uint8_t, kWaveTableSize> sine_wave_{};
  std::array<std::uint8_t, kWaveTableSize> saw_wave_{};
  std::array<std::uint8_t, kWaveTableSize> square_wave_{};
  std::array<std::uint8_t, kWaveTableSize> triangle_wave_{};
};
