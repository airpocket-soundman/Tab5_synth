#pragma once

#include "AudioSource.h"
#include "AudioSourceType.h"
#include "ExternalI2SSource.h"
#include "OnboardMicSource.h"
#include "OscillatorSource.h"
#include "SynthConfig.h"
#include "Waveform.h"

#include <array>
#include <cstddef>

class AudioEngine {
 public:
  void begin();
  void noteOnVoices(const float* note_values, std::size_t count, Waveform waveform);
  void noteOff();
  void setVolume(float volume);
  void setSourceType(AudioSourceType source_type);
  bool beginMicSampleRecording();
  void updateMicSampleRecording();
  bool finishMicSampleRecording(bool commit_sample = true);
  bool recordMicSample();
  bool hasMicSample() const;
  bool isMicRecording() const;

  [[nodiscard]] bool isNotePlaying() const;
  [[nodiscard]] bool isCurrentSourceAvailable() const;
  [[nodiscard]] bool isSourceAvailable(AudioSourceType source_type) const;
  [[nodiscard]] int activeMidiNote() const;
  [[nodiscard]] float activeFrequency() const;
  [[nodiscard]] Waveform activeWaveform() const;
  [[nodiscard]] float volume() const;
  [[nodiscard]] AudioSourceType activeSourceType() const;
  [[nodiscard]] std::size_t activeVoiceCount() const;

 private:
  static float noteValueToFrequency(float note_value);
  AudioSource& sourceFor(AudioSourceType source_type);
  const AudioSource& sourceFor(AudioSourceType source_type) const;
  void clearVoices();

  std::array<bool, SynthConfig::audio.polyphony_voices> voice_active_{};
  std::array<int, SynthConfig::audio.polyphony_voices> active_midi_notes_{};
  std::array<float, SynthConfig::audio.polyphony_voices> active_note_values_{};
  std::array<float, SynthConfig::audio.polyphony_voices> active_frequencies_{};
  Waveform active_waveform_ = Waveform::Sine;
  float volume_ = SynthConfig::audio.default_volume;
  AudioSourceType active_source_type_ = AudioSourceType::Oscillator;
  OscillatorSource oscillator_source_{};
  OnboardMicSource onboard_mic_source_{};
  ExternalI2SSource external_i2s_source_{};
};

