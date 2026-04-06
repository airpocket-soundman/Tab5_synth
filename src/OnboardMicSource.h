#pragma once

#include "AudioBusConfig.h"
#include "AudioSource.h"
#include "SampleBuffer.h"
#include "SynthConfig.h"

class OnboardMicSource : public AudioSource {
 public:
  bool begin() override;
  bool noteOn(std::size_t voice_index, float note_value, float frequency, Waveform waveform) override;
  void noteOff(std::size_t voice_index) override;
  void noteOffAll() override;
  void setVolume(float volume) override;
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
  static float noteToRatio(float note_value);
  int channelForVoice(std::size_t voice_index) const;

  bool initialized_ = false;
  bool recording_ = false;
  float volume_ = SynthConfig::audio.default_volume;
  std::size_t recorded_samples_ = 0;
  SampleBuffer sample_buffer_{};
  SampleBuffer recording_buffer_{};
};
