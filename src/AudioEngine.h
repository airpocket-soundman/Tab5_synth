#pragma once

#include "SynthConfig.h"
#include "Waveform.h"

#include <array>
#include <cstddef>
#include <cstdint>

class AudioEngine {
 public:
  void begin();
  void noteOn(int midi_note, Waveform waveform);
  void noteOff();
  void setVolume(float volume);

  [[nodiscard]] bool isNotePlaying() const;
  [[nodiscard]] int activeMidiNote() const;
  [[nodiscard]] float activeFrequency() const;
  [[nodiscard]] Waveform activeWaveform() const;
  [[nodiscard]] float volume() const;

 private:
  static constexpr std::size_t kWaveTableSize = SynthConfig::audio.wavetable_size;

  void buildWaveTables();
  void updateChannelVolume() const;
  static float midiToFrequency(int midi_note);
  const unsigned char* waveformTable(Waveform waveform) const;
  static float waveformValue(Waveform waveform, float phase);

  bool note_playing_ = false;
  int active_midi_note_ = -1;
  float active_frequency_ = 0.0f;
  Waveform active_waveform_ = Waveform::Sine;
  float volume_ = SynthConfig::audio.default_volume;
  std::array<std::uint8_t, kWaveTableSize> sine_wave_{};
  std::array<std::uint8_t, kWaveTableSize> saw_wave_{};
  std::array<std::uint8_t, kWaveTableSize> square_wave_{};
  std::array<std::uint8_t, kWaveTableSize> triangle_wave_{};
};
