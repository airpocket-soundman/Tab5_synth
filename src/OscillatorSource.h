#pragma once

#include "AudioSource.h"
#include "InstrumentTimbre.h"
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
  void setVoiceLevel(std::size_t voice_index, float level, Waveform waveform) override;
  void setInstrumentTimbre(InstrumentTimbre timbre);
  void setFilterEnabled(bool enabled);
  void setFilterParameters(float cutoff_normalized, float resonance_normalized, float mix_normalized);
  void setDistortionEnabled(bool enabled);
  void setDistortionParameters(float drive_normalized, float tone_normalized, float mix_normalized);
  void setBitcrusherEnabled(bool enabled);
  void setBitcrusherParameters(float bits_normalized, float rate_normalized, float mix_normalized);
  bool isAvailable() const override;
  AudioSourceType type() const override;

 private:
  static constexpr std::size_t kWaveTableSize = SynthConfig::audio.wavetable_size;
  static constexpr std::size_t kSineWaveTableSize = 1024;

  void buildWaveTables();
  void buildInstrumentWaveTable();
  void buildFilteredWaveTables();
  void ensureFilteredWaveTables();
  int channelForVoice(std::size_t voice_index) const;
  const unsigned char* waveformTable(Waveform waveform) const;
  static float waveformValue(Waveform waveform, float phase);

  float volume_ = SynthConfig::audio.default_volume;
  InstrumentTimbre instrument_timbre_ = InstrumentTimbre::Basic;
  bool filter_enabled_ = true;
  float filter_cutoff_normalized_ = 1.00f;
  float filter_resonance_normalized_ = 0.25f;
  float filter_mix_normalized_ = 0.45f;
  bool distortion_enabled_ = false;
  float distortion_drive_normalized_ = 0.40f;
  float distortion_tone_normalized_ = 0.55f;
  float distortion_mix_normalized_ = 0.00f;
  bool bitcrusher_enabled_ = false;
  float bitcrusher_bits_normalized_ = 1.00f;
  float bitcrusher_rate_normalized_ = 1.00f;
  float bitcrusher_mix_normalized_ = 0.00f;
  bool filter_tables_dirty_ = true;
  std::array<std::uint8_t, SynthConfig::audio.polyphony_voices> last_channel_volume_{};
  std::array<std::uint8_t, kWaveTableSize> silence_wave_{};
  std::array<std::int16_t, kSineWaveTableSize> sine_wave_16_{};
  std::array<std::uint8_t, kWaveTableSize> sine_wave_{};
  std::array<std::uint8_t, kWaveTableSize> saw_wave_{};
  std::array<std::uint8_t, kWaveTableSize> square_wave_{};
  std::array<std::uint8_t, kWaveTableSize> triangle_wave_{};
  std::array<std::uint8_t, kWaveTableSize> instrument_wave_{};
  std::array<std::uint8_t, kWaveTableSize> filtered_sine_wave_{};
  std::array<std::uint8_t, kWaveTableSize> filtered_saw_wave_{};
  std::array<std::uint8_t, kWaveTableSize> filtered_square_wave_{};
  std::array<std::uint8_t, kWaveTableSize> filtered_triangle_wave_{};
  std::array<std::uint8_t, kWaveTableSize> filtered_instrument_wave_{};
};
