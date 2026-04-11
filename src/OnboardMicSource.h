#pragma once

#include "AudioBusConfig.h"
#include "AudioSource.h"
#include "SampleBuffer.h"
#include "SynthConfig.h"

#include <array>

class OnboardMicSource : public AudioSource {
 public:
  bool begin() override;
  bool noteOn(std::size_t voice_index, float note_value, float frequency, Waveform waveform) override;
  void noteOff(std::size_t voice_index) override;
  void noteOffAll() override;
  void setVolume(float volume) override;
  void setVoiceLevel(std::size_t voice_index, float level, Waveform waveform) override;
  bool isAvailable() const override;
  AudioSourceType type() const override;

  bool beginRecording();
  void updateRecording();
  bool finishRecording(bool commit_sample = true);
  bool recordSample();
  bool hasSample() const;
  bool isRecording() const;

 private:
  bool ensureBuffer();
  bool renderPitchShiftedSample(float note_value, SampleBuffer& destination) const;
  static float sampleAtCubic(const std::int16_t* data, std::size_t length, float index);
  static float sampleAtBandlimited(const std::int16_t* data, std::size_t length, float index, float ratio);
  static float noteToRatio(float note_value);
  int channelForVoice(std::size_t voice_index) const;

  bool initialized_ = false;
  bool recording_ = false;
  bool has_backup_sample_ = false;
  std::uint32_t sample_generation_ = 0;
  float volume_ = SynthConfig::audio.default_volume;
  std::size_t recorded_samples_ = 0;
  SampleBuffer sample_buffer_{};
  SampleBuffer backup_sample_buffer_{};
  SampleBuffer recording_buffer_{};
  std::array<SampleBuffer, 8> pitched_buffers_{};
  std::array<bool, 8> pitched_cache_valid_{};
  std::array<float, 8> pitched_cache_note_{};
  std::array<std::uint32_t, 8> pitched_cache_generation_{};
};
